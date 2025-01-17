#include "carChecker.h"
#include "DEBUG_PRINTING.h"
#include "implysolver.h"
#include "newpartialsolver.h"
#include "statistics.h"
#include "utility.h"
#include <vector>
#include <iostream>
#include <stack>
#include <functional>
#include <algorithm>
#include <unordered_set>
#include <queue>
#include <tuple>
#include <chrono>
using namespace std;
using namespace std::chrono;
using clock_high = time_point<steady_clock>;

namespace car
{
    // increase one each time. monotonous
    int storage_id = 0;
    static vector<Cube> reorderAssum(const vector<Cube>& inter, const Cube &rres, const Cube &rtmp);

    Checker::Checker(Model *model, std::ostream &out, std::ofstream &trail_out, std::ofstream &dot_out, std::ofstream &dive_out, bool enable_dive, bool forward, bool evidence, int index_to_check,int convMode, int convParam, bool enable_rotate, int inter_cnt, bool inv_incomplete, bool uc_no_sort, int impMethod) : model_(model), out(out), dot_out(dot_out), dive_out(dive_out), enable_dive(enable_dive), evidence_(evidence),  trail_out(trail_out), convMode(convMode), convParam(convParam), rotate_enabled(enable_rotate), inter_cnt(inter_cnt), inv_incomplete(inv_incomplete), uc_no_sort(uc_no_sort), impMethod(impMethod)
    {
        // TODO: use propagate to accelerate
        backward_first = !forward;
        bad_ = model->output(index_to_check);
        bi_main_solver = new MainSolver(model,get_rotate(),false,uc_no_sort);
        #ifdef INC_SAT
        bi_main_solver->setIncrementalMode();
        #endif // INC_SAT
        if(!backward_first)
        {
            // only forward needs these
            bi_partial_solver = new PartialSolver(model);
            // TODO: extend to multi-properties.
            bi_start_solver = new StartSolver(model, bad_, true);
        }
        else{
            bi_partial_solver = nullptr;
            bi_start_solver = nullptr;
        }
        if (enable_dive)
        {
            assert(dive_out.is_open());
            dive_draw_head();
        }
        rotates.clear();
        rotate.clear();
        score_dicts.clear();
        score_dict.clear();
        restart_enabled = false;
        importO = false;
    }

    Checker::Checker(int time_limit, Model *model, std::ostream &out, std::ofstream &trail_out, std::ofstream &dot_out, std::ofstream &dive_out, bool enable_dive, bool forward, bool evidence, int index_to_check,int convMode, int convParam, bool enable_rotate, int inter_cnt, bool inv_incomplete, bool uc_no_sort, int impMethod) : model_(model), out(out), dot_out(dot_out), dive_out(dive_out), enable_dive(enable_dive), evidence_(evidence),  trail_out(trail_out), convMode(convMode), convParam(convParam), rotate_enabled(enable_rotate), inter_cnt(inter_cnt), inv_incomplete(inv_incomplete), uc_no_sort(uc_no_sort), impMethod(impMethod), time_limit_to_restart(time_limit)
    {
        // TODO: use propagate to accelerate
        backward_first = !forward;
        bad_ = model->output(index_to_check);
        bi_main_solver = new MainSolver(model,get_rotate(),false,uc_no_sort);
        #ifdef INC_SAT
        bi_main_solver->setIncrementalMode();
        #endif // INC_SAT
        if(!backward_first)
        {
            // only forward needs these
            bi_partial_solver = new PartialSolver(model);
            // TODO: extend to multi-properties.
            bi_start_solver = new StartSolver(model, bad_, true);
        }
        else{
            bi_partial_solver = nullptr;
            bi_start_solver = nullptr;
        }
        if (enable_dive)
        {
            assert(dive_out.is_open());
            dive_draw_head();
        }
        rotates.clear();
        rotate.clear();
        score_dicts.clear();
        score_dict.clear();
        restart_enabled = true;
        importO = false;
    }

    Checker::Checker(int time_limit, Checker* last_chker, int rememOption, Model *model, std::ostream &out, std::ofstream &trail_out, std::ofstream &dot_out, std::ofstream &dive_out, bool enable_dive, bool forward, bool evidence, int index_to_check,int convMode, int convParam, bool enable_rotate, int inter_cnt, bool inv_incomplete, bool uc_no_sort, int impMethod) : model_(model), out(out), dot_out(dot_out), dive_out(dive_out), enable_dive(enable_dive), evidence_(evidence),  trail_out(trail_out), convMode(convMode), convParam(convParam), rotate_enabled(enable_rotate), inter_cnt(inter_cnt), inv_incomplete(inv_incomplete), uc_no_sort(uc_no_sort), impMethod(impMethod), time_limit_to_restart(time_limit), last_chker(last_chker), rememOption(rememOption)
    {
        // TODO: use propagate to accelerate
        backward_first = !forward;
        bad_ = model->output(index_to_check);
        bi_main_solver = new MainSolver(model,get_rotate(),false,uc_no_sort);
        #ifdef INC_SAT
        bi_main_solver->setIncrementalMode();
        #endif // INC_SAT
        if(!backward_first)
        {
            // only forward needs these
            bi_partial_solver = new PartialSolver(model);
            // TODO: extend to multi-properties.
            bi_start_solver = new StartSolver(model, bad_, true);
        }
        else{
            bi_partial_solver = nullptr;
            bi_start_solver = nullptr;
        }
        if (enable_dive)
        {
            assert(dive_out.is_open());
            dive_draw_head();
        }
        rotates.clear();
        rotate.clear();
        score_dicts.clear();
        score_dict.clear();
        restart_enabled = true;
        importO = true;
    }

    Checker::~Checker()
    {
        clean();
        if (bi_main_solver)
        {
            delete bi_main_solver;
            bi_main_solver = nullptr;
        }
        if (bi_start_solver)
        {
            delete bi_start_solver;
            bi_start_solver = nullptr;
        }
        if (bi_partial_solver)
        {
            delete bi_partial_solver;
            bi_partial_solver = nullptr;
        }
    }

    bool Checker::check()
    {
        bool res;
        if (trivialCheck(res))
        {
            return res ? true : false;
        }

        res = car();
        if (res && ppstoped)
        {
            // should try the next strategy.
            // go out, build another checker.
            return true;
        }

        LOG("End");
        if (evidence_)
        {
            do
            {
                if (res)
                {
                    out << "0" << endl;
                    out << "b0" << endl;
                    out << "." << endl;
                    break;
                }

                print_evidence();

                if (dot_out.is_open())
                {
                    draw_graph();
                }
                if (dive_out.is_open())
                {
                    dive_mark_bad();
                    dive_draw_tail();
                }
            } while (false);
        }

        return res;
    }

    bool Checker::car()
    {
        bool res = false;
        if (initSequence(res))
            return res;
        if (restart_enabled)
        {
            // record the time that we entered.
            sat_timer = steady_clock::now();
        }

        /**
         * As to backward search,
         * @post Ob, Ub are both initialized.
         * uc of UNSAT(I, ~P) is inserted into O[0].
         * O[0] is inserted into MainSolver.
         */

        while (true)
        {
            bool direction = !backward_first;
            LOG((direction ? "F" : "B"));
            Usequence &U = direction ? Uf : Ub;

            Osequence &O = direction ? OI : Onp;
            LOG("Bi-Target:" << (direction ? "I" : "~P"));

            if (trySAT(U, &O, direction, res))
            {
                // if this PP phase ends, but has not reached the whole end.
                if (ppstoped)
                {
                    return true;
                    // continue;
                }
                return res;
            }

            if (!inv_incomplete)
            {
                if (InvFound(&O))
                    return true;
            }

        }

        // dead code. Should not reach
        return false;
    }

    bool Checker::trivialCheck(bool &res)
    {
        const Model *model = model_;
        // FIXME: fix for multiple properties.
        if (bad_ == model->true_id())
        {
            out << "1" << endl;
            out << "b"
                << "0" << endl;
            if (evidence_)
            {
                // print init state
                // FIXME: fix for latch inits.
                for (int i = 0; i < model->num_latches(); ++i)
                    out << "0";
                out << endl;
                // print an arbitary input vector
                for (int j = 0; j < model->num_inputs(); j++)
                    out << "0";
                out << endl;
            }
            out << "." << endl;
            if (verbose_)
            {
                cout << "return SAT since the output is true" << endl;
            }
            res = true;
            return true;
        }
        else if (bad_ == model->false_id())
        {
            out << "0" << endl;
            out << "b" << endl;
            out << "." << endl;
            if (verbose_)
            {
                cout << "return UNSAT since the output is false" << endl;
            }
            res = false;
            return true;
        }
        return false;
    }

    bool Checker::trySAT(Usequence &U, Osequence *O, bool forward, bool &safe_reported)
    {
        // NOTE: can eliminate initialization.
        safe_reported = false;
        Frame Otmp;
        MainSolver *main_solver = bi_main_solver;
        CARStats.count_enter_new_ronud();
        /**
         * this procedure is like the old car procedure, but the Osequence is not bound to be OI or Onegp.
         * @param missionary the state to be checked
         */
        while (State *missionary = pickState(U))
        {
            LOG("Pick " << missionary->id);
            /**
             * build a stack <state, depth, target_level>
             */
            CONTAINER stk;
            stk.push(item(missionary, 0, O->size() - 1));
            while (!stk.empty())
            {
                CARStats.count_enter_new_try_by();
                State *s;
                int dst, depth;
                std::tie(s, depth, dst) = stk.top();
                LOG("Try " << s->id << " " << dst);
                if (blockedIn(s, dst + 1, O, Otmp))
                {
                    // TODO: memorize state's blocking status. Since we do not remove UC, once blocked, forever blocked.
                    stk.pop();
                    CARStats.count_tried_before();
                    LOG("Tried before");
                    direct_blocked_counter++;
                    blocked_ids.insert(s->id);

                    int new_level = minNOTBlocked(s, dst + 2, O->size() - 1, O, Otmp);
                    if (new_level <= O->size())
                    {
                        stk.push(item(s, depth, new_level - 1));
                        LOG("Again " << s->id << " " << dst << " " << new_level - 1);
                        DIVE_DRAW(s, dst + 1, s, new_level, 1, 0);
                    }
                    else
                    {
                        DIVE_DRAW(s, dst + 1, nullptr, 0, 1, 0);
                    }
                    continue;
                }

                if (restart_enabled)
                {
                    auto now = steady_clock::now();
                    duration_high elapsed = now - sat_timer;
                    double time_delay = elapsed.count();
                    if (time_delay > time_limit_to_restart * 1000)
                    {
                        ppstoped = true;
                        return true;
                    }
                }

                if (satAssume(bi_main_solver, O, s, dst, Otmp, safe_reported))
                {
                    LOG("Succeed");
                    if (dst == -1)
                    {
                        return true;
                    }
                    State *tprime = getModel(bi_main_solver);
                    LOG("Get " << tprime->id << " " << dst);

                    updateU(U, tprime, s);

                    // NOTE: why here calculate minNOTBLOCKED, rather than next time when pop?
                    #ifdef DEPTH
                    int new_level = minNOTBlocked(tprime, max(0, int(O->size()-1-depth)), dst - 1, O, Otmp);
                    #else
                    int new_level = minNOTBlocked(tprime, 0, dst - 1, O, Otmp);               
                    #endif
                    if (new_level <= dst) // if even not one step further, should not try it
                    {
                        stk.push(item(tprime, depth + 1, new_level - 1));
                        LOG("Jump " << dst << " " << new_level - 1);
                        DIVE_DRAW(s, dst + 1, tprime, new_level, (O_level_repeat[dst] != s->id ? 0 : 3), O_level_repeat[dst] == s->id ? O_level_repeat_counter[dst] + 1 : 0);
                    }
                }
                else
                {
                    LOG("Fail ");
                    stk.pop();

                    if (safe_reported)
                        return true;

                    int new_level = minNOTBlocked(s, dst + 2, O->size() - 1, O, Otmp);
                    if (new_level <= O->size())
                    {
                        stk.push(item(s, depth, new_level - 1));
                        LOG("Again " << s->id << " " << dst << " " << new_level - 1);
                        DIVE_DRAW(s, dst + 1, s, new_level, ((dst == -1 || O_level_fresh[dst]) ? 0 : 2), (dst == -1 || O_level_fresh[dst]) ? 0 : (O_level_fresh_counter[dst]));
                    }
                    else
                    {
                        DIVE_DRAW(s, dst + 1, nullptr, 0, ((dst == -1 || O_level_fresh[dst]) ? 0 : 2), (dst == -1 || O_level_fresh[dst]) ? 0 : (O_level_fresh_counter[dst]));
                    }
                }
            }
        }

        // this is same with extend_F_sequence in old CAR.
        O->push_back(Otmp);


        spliter.push_back(State::next_id_);
        blocked_counter_array.push_back(direct_blocked_counter);
        direct_blocked_counter = 0;
        if (rotate_enabled)
            rotates.push_back(rotate);
#ifdef SCORE
        score_dicts.push_back(score_dict);
#endif // SCORE
        bi_main_solver->add_new_frame(Otmp, O->size() - 1, O, forward);
        PRINTIF_PROOF();
        return false;
    }

    /**
     * @brief
     *
     * @pre level less than check_level has been checked before
     * @param O
     * @param check_level
     * @return true : invariant found at target level
     * @return false : invariant does not exists at target level
     */
    bool Checker::InvFoundAt(Osequence &O, int check_level, int minimal_update_level, InvSolver *inv_solver)
    {
        // a portion of `InvFound()`
        if (check_level < minimal_update_level)
        {
            inv::InvAddOR(O[check_level], check_level, inv_solver);
            return false;
        }
        inv::InvAddAND(O[check_level], check_level, inv_solver);

        bool res = !inv_solver->solve_with_assumption();

        inv::InvRemoveAND(inv_solver, check_level);
        inv::InvAddOR(O[check_level], check_level, inv_solver);
        return res;
    }

    bool Checker::InvFound(Osequence *o)
    {
        Osequence &O = *o;
        bool res = false;
        // FIXME: Should we reuse inv_solver instead of recreating?
        InvSolver *inv_solver = new InvSolver(model_);
        // FIXED: shall we start from 0 or 1?
        // 0, to add O[0] into solver.

        /**
         * @brief About minimal update level:
         * 		It is related to specific O sequence.
         * 		Each time invariant check is done, minimal update level is reset to the highest level.
         * 		Each time a modification of low-level frame will possibly modify this minimal level.
         *
         */
        for (int i = 0; i < O.size(); ++i)
        {
            if (InvFoundAt(O, i, fresh_levels[o], inv_solver))
            {
#ifdef INV_PRINT
                cout << "invariant found at " << i << endl;
                for (int j = 0; j <= i; ++j)
                {
                    cout << "level = " << j << endl;
                    cout << "uc := " << endl;
                    for (auto uc : O[j])
                    {
                        cout << "(";
                        for (int k : uc)
                            cout << k << ", ";
                        cout << ")" << endl;
                    }
                }
#endif
                while (O.size() > i)
                    O.pop_back();
                res = true;
                // already found invariant.
                fresh_levels[o] = -1;
                break;
            }
        }
        // NOTE: not O.size()-1. because that level is also checked.
        fresh_levels[o] = o->size();
        delete (inv_solver);
#ifdef PRINT_INV
        cout << "END OF ONE ROUND" << endl
             << endl;
#endif
        return res;
    }

    Osequence *Checker::createOWith(State *s)
    {
        Osequence *o;
        if (SO_map.find(s) != SO_map.end())
        {
            o = SO_map[s];
        }
        else
        {
            // Cube assigns = s->s();
            // Cube neg_ass = negate(assigns);
            Frame f;
            for (auto i : s->s())
                f.push_back({-i});
            o = new Osequence({f});
            SO_map[s] = o;
            fresh_levels[o] = 0;
            // FIXME: When is the right time to add to solver? And when to set the flag?
            bi_main_solver->add_new_frame(f, o->size() - 1, o, !backward_first);
        }
        assert(o);
        return o;
    }

    // NOTE: when we try to pick a guide state, it is not used as part of the assumption but the target. Therefore, uc is not generated according to it, and therefore will not be updated.
    // Luckily, we do not use Onp or OI to guide. Therefore, here we have nothing to do with start_solver.
    State *Checker::pickState(Usequence &U)
    {
        if(pickStateLastIndex == 0)
        {
            // one round is end.
            pickStateLastIndex = U.size();
            return nullptr;
        }
        --pickStateLastIndex;
        State* s = U[pickStateLastIndex];
        if(s->is_negp)
        {
            // negp should be at position 0.
            // so when we try to fetch a state, but get negp, it is also the end of one round.
            pickStateLastIndex = U.size();
            if(bi_start_solver)
                bi_start_solver->reset();
            return nullptr;
        }
        return s;
    }


    /**
     * @brief About the principle here, see newpartialsolver.h
     *
     * @param s
     * @param prior_state
     */
    State *Checker::get_partial_state(Assignment &s, const State *prior_state)
    {
        Assignment next_assumptions = s;
        Assignment old_inputs(s.begin(), s.begin() + model_->num_inputs());

        if (prior_state)
        // it is not start state
        {
            // negate the s' as the cls, put into the solver.
            Assignment cls = negate(prior_state->s());
            bi_partial_solver->new_flag();
            bi_partial_solver->add_clause_with_flag(cls);

            // the assumption being t's input and latches.
            next_assumptions.push_back(bi_partial_solver->get_flag());
            bi_partial_solver->set_assumption(next_assumptions);

            // Therefore, it ought to be unsat. If so, we can get the partial state (from the uc)
            bool res = bi_partial_solver->solve_assumption();
            // uc actually stores the partial state.
            s = bi_partial_solver->get_conflict();

            if (res || s.empty())
            {
                // all states can reach this state! It means the counter example is found. Sure the initial state can reach this state.
                // set the next state to be the initial state.
                assert(Ub.size());
                State *init = Ub[0];
                s = init->s();
            }

            // block this clause.
            int flag = bi_partial_solver->get_flag();
            bi_partial_solver->add_clause(flag);
        }
        else
        // for initial states, there is no such "prior state", only "bad"
        {
            next_assumptions.push_back(-bad_);
            bi_partial_solver->set_assumption(next_assumptions);
            bool res = bi_partial_solver->solve_assumption();
            s = bi_partial_solver->get_conflict_no_bad(-bad_);
            // if one-step reachable, should already been found in immediate_satisfiable.
            assert(!s.empty());
        }

        // FIXME: is this `inputs` really useful?
        Assignment inputs, latches;
        for (auto &it : s)
        {
            if (abs(it) <= model_->num_inputs())
                inputs.push_back(it);
            else
                latches.push_back(it);
        }
        if (inputs.empty()) // use this inputs as the inputs.
        {
            inputs = old_inputs;
        }

        // this state is not a full state.
        State *pstate = new State(inputs, latches);
        clear_defer(pstate);
        return pstate;
    }

    /**
     * @brief print the evidence. reuse backward_first, which exactly reveals present searching direciton.
     * @pre counterexmple is met already.
     * @param 
     */
    void Checker::print_evidence() const
    {
        PRINTIF_PRIOR();

        // cout << "Counter Example is found in " << (!backward_first ? "forward" : "backward") << " search" << endl;

        // Print Backward chain first.
        bool latch_printed = false;

        out << "1" << endl;
        out << "b" << 0 << endl;
        if (counter_start_b)
        // backward_chain is not empty
        {
            State *to_print = counter_start_b;
            std::stack<std::string> helper;
            // if this is just a portion of the chain, there may not be last_inputs.
            // if(!to_print->last_inputs().empty())
            // 	helper.push(to_print->last_inputs());
            while (to_print)
            {
                State *next = prior_in_trail_b.find(to_print) != prior_in_trail_b.end() ? prior_in_trail_b.at(to_print) : nullptr;
                if (next)
                    helper.push(to_print->inputs());
                else
                    helper.push(to_print->latches());
                to_print = next;
            }
            while (!helper.empty())
            {
                out << helper.top() << endl;
                helper.pop();
            }
            latch_printed = true;
        }
        // then print forward chain.
        if (counter_start_f)
        {
            State *to_print = counter_start_f;
            if (!latch_printed)
                out << to_print->latches() << endl;
            while (to_print)
            {
                State *next = prior_in_trail_f.find(to_print) != prior_in_trail_f.end() ? prior_in_trail_f.at(to_print) : nullptr;
                out << to_print->inputs() << endl;
                to_print = next;
            }
        }
        out << "." << endl;
    }

    void Checker::draw_graph()
    {
        dot_out << "digraph SearchTree {" << endl;
        // if the counter example is found, draw the end node
        std::set<int> spliters = {spliter.begin(), spliter.end()};
        if (whichCEX())
        {
            dot_out << whichCEX()->id << " [style=filled, fillcolor=red];" << endl;
            if (backward_first)
                dot_out << whichCEX()->id << " -> NOTP [style=dashed] ;" << endl;
            else
                dot_out << "I ->" << whichCEX()->id << "[style=dashed] ;" << endl;
        }
        for (auto iter : prior_in_trail_b)
        {
            if (blocked_ids.count(iter.first->id))
                dot_out << int(iter.first->id) << " [style = dashed];" << endl;
            if (spliters.count(iter.first->id))
                dot_out << int(iter.first->id) << " [color=green];" << endl;
            else
                dot_out << int(iter.first->id) << " [color=blue];" << endl;
            if (!iter.second)
                dot_out << "I ->" << int(iter.first->id) << ";" << endl;
            else
                dot_out << int(iter.second->id) << " -> " << int(iter.first->id) << ";" << endl;
        }
        for (auto iter : prior_in_trail_f)
        {
            if (blocked_ids.count(iter.first->id))
                dot_out << int(iter.first->id) << " [style = dashed];" << endl;
            if (spliters.count(iter.first->id))
                dot_out << int(iter.first->id) << " [color=green];" << endl;
            else
                dot_out << int(iter.first->id) << " [color=red];" << endl;
            if (!iter.second)
                dot_out << int(iter.first->id) << "-> NOTP "
                        << ";" << endl;
            else
                dot_out << int(iter.first->id) << " -> " << int(iter.second->id) << ";" << endl;
        }
        dot_out << "}" << endl;
    }


    void Checker::print_flags(ostream &out)
    {
        out << endl;
        out << "------ End Printing Flags ------" << endl;
        for (auto p : SO_map)
        {
            out << "State is: " << p.first->id << endl;
            auto &flags = bi_main_solver->flag_of_O;
            if (flags.find(p.second) != flags.end())
            {
                auto &flag_for_this_O = flags[p.second];
                for (int i = 0; i < flag_for_this_O.size(); ++i)
                    out << i << ":" << flag_for_this_O[i] << ", ";
            }
            out << endl;
        }
        out << "------ End Printing Flags ------" << endl
            << endl;
    }

    bool Checker::updateU(Usequence &U, State *s, State *prior_state_in_trail)
    {
        // Counter Example Issue.
        // Every time we insert a new state into U sequence, it should be updated.
        // FIXME: without considering bi-direction, one state will only have one prior. Shall we move this into "state"?
        {
            whichPrior()[s] = prior_state_in_trail;
        }

        U.push_back(s);

        return true;
    }

    static vector<Cube> reorderAssum(const vector<Cube>& inter, const Cube &rres, const Cube &rtmp)
    {
        vector<Cube> pref;
#if defined(ASS_IRRI)
        pref = inter;
        if (pref.size() == 0)
        {
            pref = {rres, rtmp};
        }
        else
        {
            pref.insert(pref.begin() + 1, rres);
            pref.insert(pref.begin() + 2, rtmp);
        }
#ifdef PRINT_ASS
        cerr << "IRRI:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif
#elif defined(ASS_IIRR)
        pref = inter;
        if (pref.size() == 0)
        {
            pref = {rres, rtmp};
        }
        else if (pref.size() == 1)
        {
            pref.insert(pref.begin() + 1, rres);
            pref.insert(pref.begin() + 2, rtmp);
        }
        else
        {
            pref.insert(pref.begin() + 2, rres);
            pref.insert(pref.begin() + 3, rtmp);
        }
#ifdef PRINT_ASS
        cerr << "IIRR:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif
#elif defined(ASS_IRIR)
        pref = inter;
        if (pref.size() == 0)
        {
            pref = {rres, rtmp};
        }
        else if (pref.size() == 1)
        {
            pref.insert(pref.begin() + 1, rres);
            pref.insert(pref.begin() + 2, rtmp);
        }
        else
        {
            pref.insert(pref.begin() + 1, rres);
            pref.insert(pref.begin() + 3, rtmp);
        }
#ifdef PRINT_ASS
        cerr << "IRIR:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif
#elif defined(ASS_RIRI)
        pref = inter;
        if (pref.size() == 0)
        {
            pref = {rres, rtmp};
        }
        else
        {
            pref.insert(pref.begin() + 0, rres);
            pref.insert(pref.begin() + 2, rtmp);
        }
#ifdef PRINT_ASS
        cerr << "RIRI:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif
#elif defined(ASS_RRII)
        pref = inter;
        pref.insert(pref.begin() + 0, rres);
        pref.insert(pref.begin() + 1, rtmp);
#ifdef PRINT_ASS
        cerr << "RRII:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif
#elif defined(ASS_RIIR)
        pref = inter;
        if (pref.size() == 0)
        {
            pref = {rres, rtmp};
        }
        else if (pref.size() == 1)
        {
            pref.insert(pref.begin() + 0, rres);
            pref.insert(pref.begin() + 2, rres);
        }
        else
        {
            pref.insert(pref.begin() + 0, rres);
            pref.insert(pref.begin() + 3, rtmp);
        }
#ifdef PRINT_ASS
        cerr << "RIIR:" << endl;
        for (auto &cu : pref)
            for (int i : cu)
                cerr << i << ", ";
        cerr << endl;
#endif

#else
        pref = inter;
        pref.push_back(rres);
        pref.push_back(rtmp);
#endif

        return pref;
    }

    bool Checker::satAssume(MainSolver *solver, Osequence *O, State *s, int level, Frame &Otmp, bool& safe_reported)
    {
        bool forward = !backward_first;
        vector<Cube> inter;
        Cube rres, rtmp;
        Cube score_order;

        bool res = false;
        if (level == -1)
        {
            // NOTE: in backward CAR, here needs further check.
            CARStats.count_main_solver_original_time_start();
            res = lastCheck(s, O);
            if(res)
            {
                // if sat, no uc, we just update it here.
                CARStats.count_main_solver_original_time_end(res,0);
            }

            PRINTIF_SIMPLE_SAT();
        }
        else
        {
            do
            {
                // intersection:
                if (get_inter_cnt())
                {
                    // NOTE: the meaning of inter_cnt is not consistent as to ni > 1
                    const Frame &frame = level + 1 < O->size() ? (*O)[level + 1] : Otmp;
 
                    // this index is which iCube we want to create.
                    int index = 1;

                    int uc_index = frame.size();
                    // in Conv, not every UC is original UC.
                    int record_bits = conv_record[level + 1];

                    while (index <= get_inter_cnt() && frame.size() >= index)
                    {
                        Cube inter_next;

                        uc_index -= 1;
                        if (convMode >= 0)
                        {
                            // if we generated more than UC, we shall just use the original one.
                            uc_index -= (record_bits & (1 << (index - 1))) ? 1 : 0;
                            if (uc_index < 0)
                                break;
                            // cerr<<"uc index is "<< uc_index<<"/"<<frame.size()<<endl;
                            // cerr<<"index is "<<index<<", record bit is "<< ((record_bits & (1<<(index-1))) ? 1 : 0 )<<endl;
                            // cerr<<"picked uc at "<<uc_index<<endl;
                        }
                        const Cube &last_uc = frame[uc_index];

                        inter_next = s->intersect(last_uc);

                        // otherwise, do not do this!
                        #ifdef LAST_FIRST
                        if (inter_next.size() > 1)
                        {
                            // insert the last bit to the front.
                            inter_next.insert(inter_next.begin(), inter_next.back());
                            inter_next.pop_back();
                        }
                        #endif
                        inter.push_back(inter_next);
                        ++index;
                    }
                    if (inter.empty())
                        inter.push_back({});
                }
            } while (0);

            do
            {
                if(get_rotate())
                {
                // rotates[i]
                if (!rotate_enabled)
                    break;
                Cube &rcu = level + 1 < rotates.size() ? rotates[level + 1] : rotate;
                if (rcu.empty())
                {
                    rres = {};
                    rtmp = s->s();
                    // TODO: try this to be inter?
                    break;
                }

                // calculate intersection and put the others behind.
                for (int i = 0; i < rcu.size(); ++i)
                {
                    // full state
                    if (s->size() == model_->num_latches())
                    {
                        if (s->element(abs(rcu[i]) - model_->num_inputs() - 1) == rcu[i])
                            rres.push_back(rcu[i]);
                        else
                            rtmp.push_back(-rcu[i]);
                    }
                    else
                    // TODO: merge with "intersection"
                    // a partial state
                    {
                        int i = 0, j = 0;
                        while (i < s->size() && j < rcu.size())
                        {
                            if (rcu[j] == s->element(i))
                            {
                                rres.push_back(rcu[j]);
                                i++;
                                j++;
                            }
                            else
                            {
                                rtmp.push_back(s->element(i));
                                if (rcu[j] < s->element(i))
                                    j++;
                                else
                                    i++;
                            }
                        }
                    }
                }
                // inter ++ (s ∩ rcu) ++ (s - rcu) ++ s
                }
            } while (0);

            
            do
            {
            #ifdef SCORE
                // use score to order the state

                // reference to the dict.
                std::unordered_map<int, int> &dict_ref = level + 1 < score_dicts.size() ? score_dicts[level + 1] : score_dict;

                // first assign to the original order.
                score_order = s->s();

                #ifdef SCORE_REVERSE
                    // from low to high, for sanity check check.
                    std::sort(score_order.begin(), score_order.end(), [&](const int &a, const int &b)
                    {
                    if(dict_ref[a] < dict_ref[b])
                        return true;
                    else if(dict_ref[a] > dict_ref[b])
                        return false;
                    else
                    {
                        return false;
                    } });
                #else
                    // then sort according to the score.
                    // high -> low
                    // tie : ignore
                    std::sort(score_order.begin(), score_order.end(), [&](const int &a, const int &b)
                        {
                    if(dict_ref[a] < dict_ref[b])
                        return false;
                    else if(dict_ref[a] > dict_ref[b])
                        return true;
                    else
                    {
                        // tie
                        // TODO: add dealing here.

                        // at present: do not change it.
                        return false;
                    } });
                #endif

                // cerr<<"state: ";
                // for(int i:score_order)
                // cerr<<i<<"("<<dict_ref[i]<<"), ";
                // cerr<<endl;

            #endif
            } while (0);

            #ifdef SCORE
                    // NOTE: rotate and score are contradictory, because they both contain the whole state.
                    vector<Cube> pref = inter;
                    pref.push_back(score_order);
            #else
                    vector<Cube> pref = reorderAssum(inter, rres, rtmp);
            #endif

            solver->set_assumption(O, s, level, forward, pref);
            CARStats.count_main_solver_original_time_start();
            res = solver->solve_with_assumption();
            if(res)
            {
                // if sat, no uc, we just update it here.
                CARStats.count_main_solver_original_time_end(res,0);
            }
        }
        if(!res)
        {
            // update the UC.
            Cube uc = bi_main_solver->get_conflict(!backward_first);

            if (uc.empty())
            {
                safe_reported = true;
            }
            CARStats.count_main_solver_original_time_end(res,uc.size());

            addUCtoSolver(uc, O, level + 1, Otmp);
        }

        if (get_rotate() && !res)
        {
            // update rotate
            Cube &rcu = level + 1 < rotates.size() ? rotates[level + 1] : rotate;
            rcu = rres;
            rcu.insert(rcu.end(), rtmp.begin(), rtmp.end());
        }

#ifdef SCORE
        if (!res)
        {
            std::unordered_map<int, int> &dict_ref = level + 1 < score_dicts.size() ? score_dicts[level + 1] : score_dict;

    #ifdef SCORE_DECAY
            // multiplicative decay
            if (decayCounter[level + 1] == 0)
            {
                // Magic Number 20.
                decayCounter[level + 1] = 20;

                if (decayStep[level + 1] == 0)
                    decayStep[level + 1] = 1000;
                else
                    decayStep[level + 1] *= 1.01;

                // to avoid overflow
                if (decayStep[level + 1] > 1000000000)
                {
                    auto &dict = score_dicts[level + 1];
                    for (auto &pair : dict)
                    {
                        pair.second >>= 28;
                    }
                    decayStep[level + 1] = 1000;
                }
            }
            decayCounter[level + 1]--;

            // plus bumping
            for (auto &lit : s->s())
            {
    #ifdef SCORE_ABS
                dict_ref[abs(lit)] += decayStep[level + 1];
    #else
                dict_ref[lit] += decayStep[level + 1];
    #endif
            }
    #else
            for (auto &lit : s->s())
            {
    #ifdef SCORE_ABS
                dict_ref[abs(lit)]++;
    #else
                dict_ref[lit]++;
    #endif
            }
    #endif
            // cerr<<"state:";
            // for(int i:s->s())
            // cerr<<i<<", ";
            // cerr<<endl;

            // cerr<<"score:";
            // for(auto i:dict_ref)
            // cerr<<i.first <<":"<<i.second<<", ";
            // cerr<<endl;

            // TODO: As to those appear in UC, should we add more scores?
        }

#endif // !SCORE

        if(convMode >= 0)
        {
            // <others> <skipped> <previousUC>

            // whether to calculate another UC.
            bool trigger = false;
            switch(convMode)
            {
                case ConvModeAlways:
                {    
                    trigger = true;
                    // cerr<<"conv mode = Always"<<endl;
                    break;
                }
                case ConvModeHigh:
                {    
                    if(convParam <=0)
                    {
                        trigger = false;
                        break;
                    }
                    if (convParam == 1)
                    {
                        trigger = true;
                        break;
                    }
                    if(level > 0 && level > O->size() - (O->size() / convParam))
                    {
                        trigger = true;
                    }
                    else{
                        trigger = false;
                    }
                    // cerr<<"conv mode = High, res = "<<trigger<<" level="<<level<<"/"<<O->size()<<endl;
                    break;
                }
                case ConvModeLow:
                {
                    if(convParam <=0)
                    {
                        // static bool printed = false;
                        // if(!printed)
                        // {
                        //     printed = true;
                        //     cerr<<"always no"<<endl;
                        // }
                        trigger = false;
                        break;
                    }
                    if (convParam == 1)
                    {
                        // static bool printed = false;
                        // if(!printed)
                        // {
                        //     printed = true;
                        //     cerr<<"always yes"<<endl;
                        // }
                        trigger = true;
                        break;
                    }

                    if (level < 0 || level <  1 + (O->size() / convParam))
                    {
                        trigger = true;
                    }
                    else{
                        trigger = false;
                    }
                    // cerr<<"conv mode = Low, res = "<<trigger<<" level="<<level<<"/"<<O->size()<<endl;
                    break;
                }
                case ConvModeRand:
                {
                    assert(convParam != 0);
                    static mt19937 mt_rand(1);
                                
                    trigger = ((mt_rand() % convParam) == 0) ? true : false;
                    
                    // cerr<<"conv mode = Random, res = "<<trigger<<endl;
                    break;
                }
                case ConvModeStuck:
                {
                    // TODO: fill in here.
                    break;
                }

                default:
                    trigger = true;
            }

            if (!res && trigger)
            {
                // retrieve another bit.
                conv_record[level + 1] <<= 1;

                // mark as inserted
                conv_record[level + 1] += 1;

                CARStats.count_main_solver_convergence_time_start();
                // get another conflict!
                auto nextuc = solver->get_conflict_another(!backward_first);
                CARStats.count_main_solver_convergence_time_end(nextuc.size());
                //TODO: analyse, whether imply or implied.

                addUCtoSolver(nextuc, O, level + 1, Otmp);
            }
        }
        PRINTIF_QUERY();
        PRINTIF_SIMPLE_SAT();       

        return res;
    }

    State *Checker::getModel(MainSolver *solver)
    {
        bool forward = !backward_first;
        State *s = solver->get_state(forward);
        // NOTE: if it is the last state, it will be set last-inputs later.
        clear_defer(s);
        return s;
    }

    State *Checker::getModel(MainSolver *solver, State *prior)
    {
        bool forward = !backward_first;
        if (!forward)
        {
            State *s = solver->get_state(forward);
            // NOTE: if it is the last state, it will be set last-inputs later.
            clear_defer(s);
            return s;
        }
        else
        {
#ifdef PARTIAL
            Assignment full = solver->get_state_full_assignment(forward);
            return get_partial_state(full, prior);
#else
            State *s = solver->get_state(forward);
            clear_defer(s);
            return s;
#endif
        }
    }

    void Checker::clean()
    {
        for (State *duty : clear_duties)
        {
            if (duty)
            {
                delete duty;
                duty = nullptr;
            }
        }
        for (MainSolver *solver : clear_duties_mainsolver)
        {
            if (solver)
            {
                delete solver;
                solver = nullptr;
            }
        }
        for (auto &os : SO_map)
        {
            if (os.second)
            {
                if (os.second == &Onp || os.second == &OI)
                    continue;
                delete os.second;
                os.second = nullptr;
            }
        }
    }

    void Checker::clearO(State *s, Osequence *Os)
    {
        assert(s);
        assert(Os);
        Os->clear();
        delete (Os);
        SO_map.erase(s);
    }

    void Checker::insert_to_uc_index(Cube&uc, int index, int level)
    {
        if (impMethod != Imp_Sort)
            return;
        // about length based manual:
        // level+2: [0,level]
        while(uc_len_indexes.size() < level+1)
            uc_len_indexes.emplace_back((std::set<std::pair<int,int>>){});
        
        auto& index_set = uc_len_indexes[level];
        // sort according to sz
        index_set.insert({uc.size(),index});
    }

    void Checker::addUCtoSolver(Cube &uc, Osequence *O, int dst_level_plus_one, Frame &Otmp)
    {
        if (dst_level_plus_one < fresh_levels[O])
            fresh_levels[O] = dst_level_plus_one;

        Frame &frame = (dst_level_plus_one < int(O->size())) ? (*O)[dst_level_plus_one] : Otmp;

#ifdef FRESH_UC
        // To add \@ cu to \@ frame, there must be
        // 1. \@ cu does not imply any clause in \@ frame
        // 2. if a clause in \@ frame implies \@ cu, replace it by \@cu
        Frame tmp_frame;
        for (int i = 0; i < frame.size(); i++)
        {
            if (!imply(frame[i], uc, true))
            {
                tmp_frame.push_back(frame[i]);
            }
        }
        tmp_frame.push_back(uc);

        frame = tmp_frame;
#else
        frame.push_back(uc);
#endif // FRESH_UC

        if(impMethod != Imp_MOM)
            ImplySolver::add_uc(uc,dst_level_plus_one);
        else
            ImplySolver::add_uc_MOM(uc,dst_level_plus_one);

        insert_to_uc_index(uc,frame.size()-1,dst_level_plus_one);

        if (dst_level_plus_one < O->size())
            bi_main_solver->add_clause_from_cube(uc, dst_level_plus_one, O, !backward_first);
        else if (dst_level_plus_one == O->size())
        {
            if (!backward_first)
            {
                // FIXME: test me later.
                // Not always. Only if the start state is ~p.
                bi_start_solver->add_clause_with_flag(uc);
            }
        }
    }

    void Checker::updateO(Osequence *O, int dst, Frame &Otmp, bool &safe_reported)
    {
        Cube uc = bi_main_solver->get_conflict(!backward_first);

        if (uc.empty())
        {
            // this state is not reachable?
            // FIXME: fix here. It is not really blocked forever.
            PRINTIF_UNREACHABLE();
            safe_reported = true;
        }

        addUCtoSolver(uc, O, dst + 1, Otmp);

    }

    bool Checker::initSequence(bool &res)
    {
        State *init = new State(model->init());
        State *negp = new State(true);
        clear_defer(init);
        clear_defer(negp);
        Frame O0; // a frame with only one uc.

        switch (rememOption)
        {
            case(remem_None):
            {
                if (immediateCheck(init, bad_, res, O0))
                    return true;

                // forward inits.
                if (!backward_first)
                {
                    // Uf[0] = ~p;

                    // NOTE: we do not explicitly construct Uf[0] data strcutre. Because there may be too many states.  We actually need concrete states to get its prime. Therefore, we keep an StartSolver here, and provide a method to enumerate states in Uf[0].
                    // construct an abstract state
                    updateU(Uf, negp,nullptr);
                    pickStateLastIndex = Uf.size();
                    
                    // O_I
                    // NOTE: O stores the UC, while we actually use negations.
                    Frame frame;
                    for (auto lit : init->s())
                    {
                        frame.push_back({-lit});
                    }
                    OI = {frame};
                    SO_map[init] = &OI;
                }
                // backward inits.
                if (backward_first)
                {
                    // Ub[0] = I;
                    updateU(Ub, init, nullptr);
                    pickStateLastIndex = Ub.size();

                    // Ob[0] = uc0.
                    // clauses will be added by immediate_satisfible.
                    // SAT_assume(init, ~p)
                    // uc from init
                    // ~p in ~uc
                    // use uc to initialize O[0] is suitable.
                    for(int index = 0; index< O0.size(); ++index)
                    {
                        if(impMethod != Imp_MOM)
                            ImplySolver::add_uc(O0[index],0);
                        else
                            ImplySolver::add_uc_MOM(O0[index],0);
                        insert_to_uc_index(O0[index],index,0);
                    }
                    Onp = Osequence({O0});
                    SO_map[negp] = &Onp;
                }
                break;
            }

            case(remem_O0):
            {
                // If we remember all, it will be too many.
                // NOTE: sonly implement backward now.
                assert(backward_first);
                O0 = last_chker->Onp[0];
                
                if (backward_first)
                {
                    // Ub[0] = I;
                    updateU(Ub, init, nullptr);
                    pickStateLastIndex = Ub.size();

                    // Ob[0] = uc0.
                    // clauses will be added by immediate_satisfible.
                    // SAT_assume(init, ~p)
                    // uc from init
                    // ~p in ~uc
                    // use uc to initialize O[0] is suitable.

                    for(int index = 0; index< O0.size(); ++index)
                    {
                        if(impMethod != Imp_MOM)
                            ImplySolver::add_uc(O0[index],0);
                        else
                            ImplySolver::add_uc_MOM(O0[index],0);
                        insert_to_uc_index(O0[index],index,0);
                    }
                    Onp = Osequence({O0});
                    SO_map[negp] = &Onp;
                }
                break;
            }
            case(remem_short):
            {
                // If we remember all, it will be too many.
                // NOTE: sonly implement backward now.
                assert(backward_first);
                O0 = last_chker->Onp[0];
                
                sort(O0.begin(),O0.end(),[](const vector<int>& a, const vector<int>& b){return a.size() < b.size();});
                
                // int len = 0;
                // for(auto &uc:O0)
                //     len+=uc.size();
                // cerr<<"before: sz = "<<O0.size()<<", avg = "<<len/O0.size()<<endl;

                static float portion = 2;
                O0.resize(O0.size() * (1-(1/portion)));
                portion++;

                // int after_len = 0;
                // for(auto &uc:O0)
                //     after_len+=uc.size();

                // cerr<<"after: sz = "<<O0.size()<<", avg = "<<after_len/O0.size()<<endl;

                if (backward_first)
                {
                    // Ub[0] = I;
                    updateU(Ub, init, nullptr);
                    pickStateLastIndex = Ub.size();

                    // Ob[0] = uc0.
                    // clauses will be added by immediate_satisfible.
                    // SAT_assume(init, ~p)
                    // uc from init
                    // ~p in ~uc
                    // use uc to initialize O[0] is suitable.

                    for(int index = 0; index< O0.size(); ++index)
                    {
                        if(impMethod != Imp_MOM)
                            ImplySolver::add_uc(O0[index],0);
                        else
                            ImplySolver::add_uc_MOM(O0[index],0);
                        insert_to_uc_index(O0[index],index,0);
                    }
                    Onp = Osequence({O0});
                    SO_map[negp] = &Onp;
                }
                break;
            }    

            case(remem_Ok):
            {
                int boundK = 3;
                // If we remember all, it will be too many.
                // NOTE: sonly implement backward now.
                assert(backward_first);
                
                if (backward_first)
                {
                    // Ub[0] = I;
                    updateU(Ub, init, nullptr);
                    pickStateLastIndex = Ub.size();

                    Onp = last_chker->Onp;
                    Onp.resize(boundK);
                    SO_map[negp] = &Onp;

                    for(int findex = 0; findex < Onp.size(); ++findex)
                    {
                        auto &frame = Onp[findex];
                        for(int index = 0; index < frame.size(); ++index)
                        {
                            if(impMethod != Imp_MOM)
                                ImplySolver::add_uc(frame[index],findex);
                            else
                                ImplySolver::add_uc_MOM(frame[index],findex);
                            insert_to_uc_index(frame[index], index, findex);
                        }
                        bi_main_solver->add_new_frame(Onp[0], Onp.size() - 1, &Onp, false);
                    }

                    if(get_rotate())
                    {
                        rotates.push_back(init->s());
                    }
                    return false;
                }
                break;
            }

            case(remem_Uk):
            {
                assert(backward_first);
                delete init;
                auto *&last_init = last_chker->Ub[0];
                auto * init = new State(last_init->inputs_vec(), last_init->s());
                clear_defer(init);

                if (immediateCheck(init, bad_, res, O0))
                    return true;

                updateU(Ub, init, nullptr);
                // backward inits.
                // Ub[0] = I;   
                for(car::State *st: last_chker->Ub){
                    if(!st)
                        break;
                    // all its followings
                    if(last_chker->whichPrior()[st] == init)
                    {
                        State* s = new State(st->inputs_vec(), st->s());
                        clear_defer(s);
                        updateU(Ub, s, init);
                    }
                }

                pickStateLastIndex = Ub.size();

                // Ob[0] = uc0.
                // clauses will be added by immediate_satisfible.
                // SAT_assume(init, ~p)
                // uc from init
                // ~p in ~uc
                // use uc to initialize O[0] is suitable.
                for(int index = 0; index< O0.size(); ++index)
                {
                    if(impMethod != Imp_MOM)
                        ImplySolver::add_uc(O0[index],0);
                    else
                        ImplySolver::add_uc_MOM(O0[index],0);
                    insert_to_uc_index(O0[index],index,0);
                }
                Onp = Osequence({O0});
                SO_map[negp] = &Onp;
                break;
            }

        default:
            break;
        }



        if(get_rotate())
        {
            rotates.push_back(init->s());
        }

        if (backward_first)
            bi_main_solver->add_new_frame(Onp[0], Onp.size() - 1, &Onp, false);
        if (!backward_first)
            bi_main_solver->add_new_frame(OI[0], OI.size() - 1, &OI, true);



        return false;
    }

    //////////////helper functions/////////////////////////////////////////////

    // NOTE: if not updated, it return the same state all the time?
    State *Checker::enumerateStartStates(StartSolver *start_solver)
    {
// partial state:
#ifdef PARTIAL
        if (start_solver->solve_with_assumption())
        {
            Assignment ass = start_solver->get_model();
            ass.resize(model_->num_inputs() + model_->num_latches());
            State *partial_res = get_partial_state(ass, nullptr);
            clear_defer(partial_res);
            return partial_res;
        }
#else
        if (start_solver->solve_with_assumption())
        {
            State *res = start_solver->create_new_state();
            clear_defer(res);
            return res;
        }
#endif
        return NULL;
    }

    // This is used in sequence initialization
    bool Checker::immediateCheck(State *from, int target, bool &res, Frame &O0)
    {
        // bi_main_solver.
        auto &solver = bi_main_solver;
        // NOTE: init may not be already set.
        vector<int> latches = from->s();

        int last_max = 0;
        do
        {
            CARStats.count_main_solver_original_time_start();
            if (solver->solve_with_assumption(latches, target))
            {
                CARStats.count_main_solver_original_time_end(true,0);
                // if sat. already find the cex.
                State *s = solver->get_state(true); // no need to shrink here
                clear_defer(s);
                whichPrior()[s] = from;
                whichCEX() = s;
                res = false;
                return true;
            }
            // NOTE: the last bit in uc is added in.
            Cube cu = solver->get_conflict_no_bad(target); // filter 'bad'
            CARStats.count_main_solver_original_time_end(false,cu.size());
            
            if (cu.empty())
            {
                // this means, ~p itself is bound to be UNSAT. No need to check.
                res = true;
                return true;
            }

            if(convMode==0 || convMode == 1)
            {
                // this time's first lit.
                if (abs(cu[0]) <= last_max)
                    break;
                else
                {
                    last_max = abs(cu[0]);
                    O0.push_back(cu);
                }
                auto unfresh = [&cu, &last_max](int x)
                {
                    return abs(x) > last_max;
                };
                std::stable_partition(latches.begin(), latches.end(), unfresh);
            }
            else{
                O0.push_back(cu);
                break;
            }
        } while (true);
        return false;
    }

    bool Checker::lastCheck(State *from, Osequence *O)
    {
        // if(SO_map[State::negp_state] != O)
        // 	return true;
        bool direction = !backward_first;
        if (direction)
        // in forward CAR, last target state is Init. Initial state is a concrete state, therefore it is bound to be reachable (within 0 steps). Here we only need to update the cex structure.
        {
            whichCEX() = from;
            return true;
        }
        else
        // in backward car, we use uc0 instead of ~P to initialize Ob[0]. This makes it possible to return false, because uc0 is necessary but not essential.
        {
            // check whether it's in it.
            bool res = bi_main_solver->solve_with_assumption(from->s(), bad_);
            if (res)
            {
                // OK. counter example is found.
                State *s = bi_main_solver->get_state(direction);
                clear_defer(s);
                whichPrior()[s] = from;
                whichCEX() = s;
                return true;
            }
        }
        return false;
    }

    void Checker::print_sat_query(MainSolver *solver, State *s, Osequence *o, int level, bool res, ostream &out)
    {
        static int sat_cnt = 0;
        out << endl;
        out << "[SAT] times: " << ++sat_cnt << "\tlevel=" << level << endl;
        //<<"\tflag="<<solver->flag_of(o,level)<<endl;
        out << "SAT solver clause size: " << solver->size() << endl;
        out << "State: ";
        for (int i : s->s())
            out << i << ", ";
        out << endl;
        // cout<<"state: \n\tlatches: "<<s->latches()<<"\n\tinputs: "<<s->inputs()<<endl;
        // solver->print_clauses();
        solver->print_assumption(out);
        out << "res: " << (res ? "SAT" : "UNSAT") << endl;
        out << "-----End of printing-------" << endl;
    }

    /**
     * @brief
     * @pre s->s() is in abs-increasing order
     *
     * @param s
     * @param frame_level
     * @param O
     * @param Otmp
     * @return true
     * @return false
     */
    bool Checker::blockedIn(State *s, const int frame_level, Osequence *O, Frame &Otmp)
    {
        bool res = false;
        switch (impMethod)
        {
            case(Imp_Manual):
            {
                Frame &frame = (frame_level < O->size()) ? (*O)[frame_level] : Otmp;
                for (const auto &uc : frame)
                {
                    CARStats.count_1_begin();
                    res = s->imply(uc);
                    CARStats.count_1_end(res);
                    if (res)
                    {
                        break;
                    }
                }
                break;
            }

            case(Imp_Solver):
            {
                res = ImplySolver::is_blocked(s,frame_level);
                break;   
            }

            case(Imp_Sample):
            {
                // -1: not decided
                // 0: use solver
                // 1: manually 

                if(imply_decision != 1)
                {
                    // use solver
                    if(imply_decision == -1)
                        CARStats.count_imply_dec_begin();
                    res = ImplySolver::is_blocked(s,frame_level);
                    if(imply_decision == -1)
                        CARStats.count_imply_dec_end(1);
                }

                // manually.
                if(imply_decision != 0)
                {
                    if(imply_decision == -1)
                        CARStats.count_imply_dec_begin();
                    Frame &frame = (frame_level < O->size()) ? (*O)[frame_level] : Otmp;
                    for (const auto &uc : frame)
                    {
                        res = s->imply(uc);
                        if (res)
                        {
                            break;
                        }
                    }
                    if(imply_decision == -1)
                        CARStats.count_imply_dec_end(2);
                }
                // do dicision
                if(imply_decision == -1)
                    imply_decision = CARStats.imply_dec_decide();
                
                break;
            }

            case(Imp_Sort):
            {
                if(frame_level+1 > uc_len_indexes.size())
                {
                    res = false;
                    break;
                }
                auto& helper = uc_len_indexes[frame_level];
                Frame &frame = (frame_level < O->size()) ? (*O)[frame_level] : Otmp;
                for (auto& pr: helper)
                {
                    int index = pr.second;
                    const auto& uc = frame[index];
                    res = s->imply(uc);
                    if (res)
                    {
                        break;
                    }
                }
                break;
                // assert(frame.size() == helper.size());
            }

            case (Imp_Exp):
            {
                // count for sz and winning rate.

                // solver
                clock_high begin = steady_clock::now();
                res = ImplySolver::is_blocked(s,frame_level);
                clock_high end = steady_clock::now();
                duration_high elapsed = end - begin;
                double time_delay_for_solver = elapsed.count(); 

                // manual
                begin = steady_clock::now();
                
                Frame &frame = (frame_level < O->size()) ? (*O)[frame_level] : Otmp;
                for (const auto &uc : frame)
                {
                    res = s->imply(uc);
                    if (res)
                    {
                        break;
                    }
                }
                end = steady_clock::now();
                elapsed = end - begin;
                bool solver_win = elapsed.count()> time_delay_for_solver ? true : false;
                CARStats.record_winner(solver_win,frame.size());

                break;
            }

            case (Imp_Thresh):
            {
                Frame &frame = (frame_level < O->size()) ? (*O)[frame_level] : Otmp;
                if(frame.size() > 10000)
                {
                    res = ImplySolver::is_blocked(s,frame_level);
                }
                else{
                    for (int i = frame.size()-1; i>=0; --i)
                    {
                        const auto &uc = frame[i];
                        res = s->imply(uc);
                        if (res)
                        {
                            break;
                        }
                    }
                }
                break;
            }

            case (Imp_MOM):
            {
                res = ImplySolver::is_blocked_MOM(s,frame_level);
                break;
            }

            default:
                break;
        }
        return res;
    }

    /**
     * @brief Find minimal level among [min,max] that s is not blocked in. 
     * 
     * @param s 
     * @param min 
     * @param max 
     * @param O 
     * @param Otmp 
     * @return int 
     */
    int Checker::minNOTBlocked(State *s, const int min, const int max, Osequence *O, Frame &Otmp)
    {
        CARStats.count_imply_begin();
        int start = min;
        while (start <= max)
        {
            if (!blockedIn(s, start, O, Otmp))
            {
                break;
            }
            start++;
        }
        CARStats.count_imply_end();
        return start;
    }


    namespace inv
    {
        /**
         * @brief
         * Add the negation of this frame into the solver
         * @param frame
         */
        void InvAddOR(Frame &frame, int level, InvSolver *inv_solver_)
        {
            inv_solver_->add_constraint_or(frame, level);
        }

        /**
         * @brief
         * Add the real states into this solver.
         * @param frame
         */
        void InvAddAND(Frame &frame, int level, InvSolver *inv_solver_)
        {
            inv_solver_->add_constraint_and(frame, level);
        }

        /**
         * @brief pop the last lit of assumption, and negate it.
         *
         */
        void InvRemoveAND(InvSolver *inv_solver_, int level)
        {
            inv_solver_->release_constraint_and(level);
        }

        bool InvFoundAt(Fsequence &F_, const int frame_level, InvSolver *inv_solver_, int minimal_update_level_)
        {
            if (frame_level <= minimal_update_level_)
            {
                InvAddOR(F_[frame_level], frame_level, inv_solver_);
                return false;
            }
            InvAddAND(F_[frame_level], frame_level, inv_solver_);
            bool res = !inv_solver_->solve_with_assumption();
            InvRemoveAND(inv_solver_, frame_level);
            InvAddOR(F_[frame_level], frame_level, inv_solver_);
            return res;
        }

        // -1 for not found.
        // >=0 for level
        int InvFound(Model *model_, Fsequence &F_, int &minimal_update_level_, int frame_level)
        {
            if (frame_level == 0)
                return -1;
            int res = -1;
            InvSolver *new_inv_solver_ = new InvSolver(model_);
            for (int i = 0; i < frame_level; i++)
            {
                if (InvFoundAt(F_, i, new_inv_solver_, minimal_update_level_))
                {
                    res = i;
                    // delete frames after i, and the left F_ is the invariant
                    // NOTE: this is temporarily blocked for testing incremental-enumratiing-start-solver.
                    // while (F_.size() > i + 1)
                    // {
                    // 	F_.pop_back();
                    // }
                    break;
                }
            }
            delete new_inv_solver_;
            return res;
        }
    };
}