
//testWitBaye.h

#pragma once
#include <queue>
#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
//extern std::vector<std::vector<double>> ruleUpdates;  // declare shared container // uncomment
extern bool backtrack_on = false;
extern double backtrack_prob = 0.0;

// forward declare — we only need to know it exists
enum WitnessAction;

struct WitnessMove 
{
    std::pair<int, int> posBefore;
    std::pair<int, int> posAfter;
    WitnessAction action;
    bool isBacktrack = false;
};


extern WitnessAction backtrack_act = kWitnessActionCount; // which is just the count and is an "out of bounds" act if I try to use it

// shared queue between Witness.h and Driver.cpp
inline std::queue<WitnessMove> witnessMoveQueue;


// pretty print for WitnessMove
inline std::ostream& operator<<(std::ostream& os, const WitnessMove& m) 
{
    os << "( " << m.posBefore.first << "," << m.posBefore.second
       << " -> " << m.posAfter.first << "," << m.posAfter.second
       << ",\t[b]=" << m.isBacktrack
       << ", action=" << (int)m.action << " )";
    return os;
}

// template for debugging queues
template <typename T>
void printQueue(std::queue<T> q) // copy on purpose
{ 
    
    while (!q.empty()) 
    {
        std::cout << "\n[Call from Driver.cpp] Queue:\n";
        std::cout << q.front() << "\n";
        backtrack_act = witnessMoveQueue.front().action; // gives target move when you partially leave a node
        q.pop();
    }
}


inline void UpdateProbTableAndLog(
    const std::vector<int>& possibleDirs,
    const std::vector<std::vector<std::string>>& MoveTable,
    std::vector<std::vector<double>>& ProbTable,
    int RULE_ROW,
    int MOVE_COL,
    std::string& lastBlock
)
{
    std::stringstream ss_rules;

    
    for(int i=0; i<RULE_ROW; i++)
    {
        int must_count = 0;
        int cant_count = 0;

        std::vector<int> must_dirs;
        std::vector<int> cant_dirs;
        std::vector<int> neutral_dirs;

        double slip_logic = 0.05;
        double slip_mech = 0.01;
        double slip_total = slip_logic+slip_mech; // total slip probability to distribute // both mechanical and logical

        // getting all the directions and their status for rule i
        for(int j=0; j<MOVE_COL; j++)
        {
            if(MoveTable[i][j] == "M")
            {
                must_count++;
                must_dirs.push_back(j);
            }
            else if(MoveTable[i][j] == "C")
            {
                cant_count++;
                cant_dirs.push_back(j);
            }
            else if(std::find(possibleDirs.begin(), possibleDirs.end(), j) != possibleDirs.end())
            {
                neutral_dirs.push_back(j);
            }
        }

        // prob depends on invalid/valid path 
        // must dir needs to be 1-sum // sum is 0.06*number of available dirs
        // cant dir needs to be 0.06 each, neutral gets the rest divided
        // if multiple must dirs, all slip prob
        if(must_count > 1) // invalid for that rule
        {
            //std::cout << "\nmust_count: " << must_count; 
            // set all possibleDirs to slip total
            for(int dir : possibleDirs)
            {
                ProbTable[i][dir] = slip_total; // slip probability for all must_take violated dirs
            }
            // CHECK FOR BACKTRACK case here
        }
        else if(must_count == 1) // valid for that rule
        {
            double must_sum = 0.0; // remaining prob after sum of all slips
            if(neutral_dirs.size() > 0)
            {
                for(int dir : neutral_dirs)
                {
                    ProbTable[i][dir] = slip_total; 
                    must_sum += ProbTable[i][dir];
                }
            }

            if(cant_dirs.size() > 0)
            {
                for(int dir : cant_dirs)
                {
                    ProbTable[i][dir] = slip_total; 
                    must_sum += ProbTable[i][dir];
                }
            }
            if(must_sum == 0.0)
                must_sum = 0.05; // dont allow full prob to must dir
            
            // assign remaining prob to must dir
            ProbTable[i][must_dirs[0]] = 1.0 - must_sum; 
        }
        else if(cant_count > 0 && must_count == 0) // not immediately valid or invalid for that rule
        {
            double neutral_sum = 0.0; // remaining prob after sum of all slips
            for(int dir : cant_dirs)
            {
                ProbTable[i][dir] = slip_total; 
                neutral_sum += ProbTable[i][dir];
            }

            ss_rules << "\nneutral_sum: " << neutral_sum;

            // for the case where we have neutral directions too (and not all cant dirs)
            if(neutral_dirs.size() > 0)
            {
                double neutral_divide = (1.0 - neutral_sum) / (double)neutral_dirs.size();
                ss_rules << "\nneutral_divide: " << neutral_divide;
                for(int dir : neutral_dirs)
                {
                    ProbTable[i][dir] = neutral_divide; 
                }
            }
        }
    }

    /*

    // print prob table to stringstream
    ss_rules << "\n\n\t testWitBaye.h [Probability Table]\n\tUp\tDown\tLeft\tRight\tStr\tEnd\n";
    for (int i = 0; i < RULE_ROW; i++)
    {
        ss_rules << "R-" << i << ":\t";
        for (int j = 0; j < MOVE_COL; j++)
            ss_rules << ProbTable[i][j] << "\t";
        ss_rules << std::endl;
    }

    lastBlock = ss_rules.str();
    */
}

static WitnessAction lastAction = WitnessAction::kWitnessActionCount; // set to count, which we'll treat as a bogus value


static std::pair<int, int> beforeNode = {-1, -1};

inline void newMakeTruthTable(
    int count_toggle,
    std::pair<int, int> before
)   
{

    if(witnessMoveQueue.empty())
        return;

    std::cout << "\n[testWitBaye.h] newMakeTruthTable # "<< count_toggle;
    std::cout << "\nlast node: " << before.first << ", " << before.second;
    // TO BE IMPLEMENTED LATER



    
    
}




void oldPrintTruthTable(const std::vector<int>& activeRows)
{
    int N = activeRows.size();
    int total = 1 << N; // 2^N combinations

    // headers/ rules
    std::cout << "";
    for (int i = 0; i < N; ++i) {
        std::cout << "R" << activeRows[i] << "\t";
    }
    std::cout << "\n";

    // truth table combos
    for (int mask = 0; mask < total; ++mask) {
        for (int i = 0; i < N; ++i) {
            int bit = (mask >> i) & 1;
            std::cout << bit << "\t";
        }
        std::cout << "\n";
    }
}


void PrintTruthTable(
    const std::vector<int>& activeRows,
    const std::vector<std::vector<double>>& table,
    int N_dirs = 0,
    int N_pSum = 0,
    int N_joint = 0,
    int precision_adjust = 2
)
{
    int N = activeRows.size();
    int total = table.size();

    std::cout << "\n";
    // header
    for (int i = 0; i < N; ++i)
        std::cout << "R" << activeRows[i] << "\t";

    for (int d = 0; d < N_dirs; ++d)
        std::cout << "d_" << d << "\t";

    for (int e = 0; e < N_pSum; ++e)
    {
        std::cout << "d_Sum" << "\t";
        if(e < N_pSum)
        {
            std::cout << "1-S" << "\t";
            e++;
        }

    }

    for (int j = 0; j < N_joint; ++j)
        std::cout << "Joint" << "\t";

    std::cout << "\n";

    // each row
    for (int r = 0; r < total; ++r)
    {
        for (int c = 0; c < N + N_dirs + N_pSum + N_joint; ++c)
        {
            if(table[r][c] == 0.0 || table[r][c] == -1.0)
                std::cout << "_" << "\t";
            else
                std::cout << std::setprecision(precision_adjust) << table[r][c] << "\t";
        }

        std::cout << "\n";
    }
}

static double array_priors[6] = {0.2, 0.2, 0.2, 0.2, 0.2, 0.2};

static int toggleMakeTruthTable = 0;

static double update_rule_0 = 0.0; // testing rule0 update

inline void MakeTruthTable(
    const std::vector<int>& possibleDirs,
    const std::vector<std::vector<std::string>>& MoveTable,
    const std::vector<std::vector<double>>& ProbTable,
    int RULE_ROW,
    int MOVE_COL,
    std::string& lastBlock,
    int count_toggle
)
{

    /*
        // possibleDirs is all the valid directions we have at the current node
        // MoveTable - string 2d vector showing all the M and C status of rules
        // ProbTable - the probability table from that MoveTable
        // RULE_ROW - rows for all rules (total) so 6
        // MOVE_COL - cols based on all directions so 6
        // lastBlock - contains the last output from the Movetable I think? To show one output instead of incessant ones
        // count_toggle - same function as lastBlock 
    */

    int precision_adjust = 4;

    if (count_toggle != 0) return; // this needs to be reset when we move to a new node

    std::cout << "\n\n\t[testWitBaye.h] Calling the truth table # " << count_toggle << "\n";

    // all MakeTruthTable info
    std::stringstream showTTinfo;


    // slip probabilities
    double slip_logic = 0.05; // logical slip - which happens when we deviate from a rule we may know well
    double slip_mech = 0.01; // mechanical slip - which may happen organically as we interact with the game/ UI

    // get active rows for curr node
    std::vector<int> activeRows;

    for (int row = 0; row < RULE_ROW; ++row) 
    {
        bool valid = false;
        for (int col = 0; col < MOVE_COL; ++col) 
        {
            if (ProbTable[row][col] != -1) 
            {
                // a bad way to check if a row/ rule has any significant values
                // if it does, then we track it in activeRows and break this loop
                valid = true;
                break;
            }
        }
        if (valid) 
            activeRows.push_back(row);
    }

    // bitmask fpr truth table
    int N_rules = activeRows.size(); // to decide how many bits we'll need for the ACTIVE RULES
    int total_rows = 1 << N_rules; // total_rows = 2^N_rules (e.g., 3 rules = 8 possible T/F combinations)

    // from prev example: 8 rows [total_rows] x 3 cols [N_rules]
    // T/F table for active rules
    std::vector<std::vector<double>> truthTable(total_rows, std::vector<double>(N_rules)); 

    for (int mask = 0; mask < total_rows; ++mask) 
    {
        for (int i = 0; i < N_rules; ++i) 
        {
            // truthTable[mask][i] = (mask >> i) & 1;
            // getting only the T/F part
            truthTable[mask][i] = static_cast<double>((mask >> i) & 1); 
        }
    }

    // print // comment out later
    //PrintTruthTable(activeRows);
    //PrintTruthTable(activeRows, truthTable); //shows the TT combos only

    // to help with indexes
    int N_dirs = 6; 
    int N_prob_sum = 2;
    int N_joint = 1; 
    // total cols I'll need is // N_rules + N_dirs + N_prob_sum + N_joint
    int N_totalCols = N_rules + N_dirs + N_prob_sum + N_joint;

    // build full table
    std::vector<std::vector<double>> fullTT(total_rows, std::vector<double>(N_totalCols, -1.0));

    // fill in the rule bit columns from truthTable [000, 001, 010, ...]
    for (int r = 0; r < total_rows; ++r) 
    {
        for (int c = 0; c < N_rules; ++c) 
        {
            fullTT[r][c] = truthTable[r][c]; // just copying truthTable - I can use the status of the rule [0 or 1] to check later
        }
    }





    // adjusting all cols/ dirs to show the smallest probability vals only
    // then if probTable [row][col] IS NOT -1 then add it to the fullTT 
    for (int r = 0; r < total_rows; ++r) // for each truth-table row
    {   
        bool anyActive = false; // is any rule active - checking for row 0 basically 
        //double dir_sum = 0.0; // sum of all probabilities for a row // with multiple or one rule active

        int count_activeRules = 0; // to adjust the logical slip 

        

        int count_0_06 = 0;     // number of 0.06 values among active rules


        //std::cout << "\n\n\npre 1st pass\n\n\n";
        //PrintTruthTable(activeRows, fullTT, N_dirs, N_prob_sum, N_joint, 1);

        // 1st pass: count active rules and 0.06 probabilities
        for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx) 
        {
            if (fullTT[r][ruleIdx] == 1) 
            {
                anyActive = true; // this rule at r is active if true
                int rowID = activeRows[ruleIdx];
                for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol) 
                {
                    // fetching the prob from the ProbTable based on the RULE/rowID and the relevant dir/move/col
                    double val = ProbTable[rowID][dirCol]; 

                    if (val == (slip_logic+slip_mech)) // count all slip cols/ dirs
                    { 
                        count_0_06++;
                    }
                }
            }
        }


        //std::cout << "\n\n\n1st pass\n\n\n";
        //PrintTruthTable(activeRows, fullTT, N_dirs, N_prob_sum, N_joint, 1);

        // 2nd pass: fill in probabilities and adjust 0.06 values
        if (anyActive) 
        {
            for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx) // 3 rules -> so 0 to 3 times
            {
                if (fullTT[r][ruleIdx] == 1) // if a certain rule is active
                {
                    int rowID = activeRows[ruleIdx]; // ruleID
                    for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol) 
                    {
                        double val = ProbTable[rowID][dirCol];
                        double& val2 = fullTT[r][N_rules + dirCol]; // ref to fullTT cell // N_rules + dirCol gives me the exact dir prob I need

                        if (val != -1) 
                        {

                            double decimal_places = 1000.0;

                            if (val == slip_logic+slip_mech && count_0_06 > 0) // a bit redundant but we're checking to see how much we need to spread out the slip values
                            {
                                // adjust 0.06 proportionally by number of occurrences/ ruleas active
                                val2 = slip_logic / count_0_06 + slip_mech;
                                val2 = std::round(val2 * decimal_places) / (double)decimal_places;
                            } 
                            else if (val2 < 0.0) 
                            {
                                // first time assigning a non-zero value to our table - fullTT
                                val2 = val;
                                val2 = std::round(val2 * decimal_places) / (double)decimal_places;
                            } 
                            else 
                            {
                                // not a slip dir [0.06] and neither is it an invalid/ blocked one [-1]
                                // e.g. 0.47 
                                // if multiple rules affect this dir, take the minimum
                                val2 = std::min(val2, val);
                                val2 = std::round(val2 * decimal_places) / (double)decimal_places;
                            }
                        }
                    }
                }
            }
        }



        //std::cout << "\n\n\n2nd pass\n\n\n";
        //PrintTruthTable(activeRows, fullTT, N_dirs, N_prob_sum, N_joint, 1);






        double decimal_places = 1000.0;

        // this is for the guess case
        double guess_val = 0.0; // we'll use this to check if a probab value is too small or not 
        // if no rules were active, assign uniform probability -> this is for [guess] case
        if (!anyActive)
        {
            for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol)
            {
                if(std::find(possibleDirs.begin(), possibleDirs.end(), dirCol) != possibleDirs.end())
                {
                    //fullTT[r][N + dirCol] = 1.0 / possibleDirs.size();
                    fullTT[r][N_rules + dirCol] = std::round((1.0 / possibleDirs.size()) * decimal_places) / decimal_places; // 2 decimals
                    guess_val = fullTT[r][N_rules + dirCol];
                }
                    
            }
        }

        /*
            // so far we've adjusted the 0.06 vals
            // we've accounted for 0.06 if multiple rules are active
            // accounted for the 0.47 or significant probs too
            // and we've assigned the values for the guess row/ case
        */


        // BIG sum CHECK // d_Sum


        
        // to check if all directional probabs sum to 1.0 or 0.99
        // populate the sum col
        double prob_sum = 0.0;
        std::vector<int> bigCols;
        for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol)
        {
            double v_dir = fullTT[r][N_rules + dirCol]; // prob val for that dir
            if (v_dir != -1) // if it's an active dir/col
            {
                prob_sum += v_dir;

                if(prob_sum <= 1.0 && prob_sum >= 0.99) // for guess case 
                    prob_sum = 1.0;
            }
        }

        fullTT[r][N_rules + MOVE_COL] = prob_sum; // to calc d_Sum

        backtrack_prob = 1.0 - prob_sum; // I still need to adjust the joint probab for this
        fullTT[r][N_rules + MOVE_COL + 1] = backtrack_prob; // for 1-S // this shows the remaining prob if the sum isn't 1.0

        // TODO backtrack_prob

        // write Joint later //??

      

        if (prob_sum < 0.99 && prob_sum > (double)possibleDirs.size()*(slip_logic+slip_mech)) // sliptotal * 3
        {
            double dir_sum_ = 0.0; // to get sum of all small cols/ dirs
            std::vector <int> bigCols; // stores the dir/ col of bigger (>0.25) cols

            for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol)
            {
                double dir_p = fullTT[r][N_rules+dirCol]; // directional prob at col [at this point]
                
                if(dir_p != -1)
                {
                    if(dir_p <= (slip_logic+slip_mech)) // if it's equal to say 0.06 or 0.035
                    {
                        dir_sum_ += dir_p; // to get sum of all small cols/ dirs
                    }

                    else 
                        bigCols.push_back(dirCol); // to track the big/ significant cols

                }
            }

            if(!bigCols.empty()) // we have stuff like 0.47
            {
                double adjust_prob = (1.0 - dir_sum_) / (double)bigCols.size(); // on the rare case that we have two big prob cols (0.47 & 0.47) 
                // does that even exist // why can't I find a case that fits
                // only the 2 must_crosses can give that - which is invalid and we fit all cols to show slip_total/ 0.06
                // otherwise any big cols are adjusting values from the cant/ neutral cases

                if(bigCols.size() > 1)
                    std::cout << "\n\t\t\t\t-------[error case??] bigCols_size = " << bigCols.size() << "-------";

                double temp_sum = dir_sum_; // gets the sum of all small probs

                for(auto item: bigCols)
                {
                    // comment
                    // for every item/col in bigCols
                    // I have to add more comments when I'm tired/ stupider
                    // we can't keep doing this on no sleep // remove comments later
                    showTTinfo << "\t in bigCols: " << item; // item is the col/ dir
                    fullTT[r][N_rules + item] = adjust_prob;

                    temp_sum += adjust_prob; // add sum for each big col

                }

                fullTT[r][N_rules + MOVE_COL] = temp_sum; // making sure all big Cols and small cols have the total sum saved in the relevant col
            }
        }

    }

    // if sum != 1.0 or 0.99



    int count_pri = 0;

    // FIX THESE
    // ruleJoint and 
    //+1 for 1-S or 1-remaining_prob_sum
    PrintTruthTable(activeRows, fullTT, N_dirs, N_prob_sum, N_joint, 4);
    std::vector<std::vector<double>> ruleJointSums(N_rules, std::vector<double>(MOVE_COL+1, 0.0)); // +1 for the [unknown] backTrack dir
    std::vector<double> jointSums(MOVE_COL+1, 0.0); // sum of all joint values per direction

    // directional updates
    showTTinfo << "\n____________________\n";
    showTTinfo << "\n[Rule Joint Table with Updates]\n";


    std::cout << "\n\ntarget move: " << backtrack_act;

    // I need to add something here so calc the backtrack probs
    // TODO


    // headers
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
        showTTinfo << "R" << activeRows[ruleIdx] << "\t";
    for (int d = 0; d < MOVE_COL; d++)
        showTTinfo << "Jd" << d << "\t";
    showTTinfo << "Jd[Back]" << "\t";
    showTTinfo << "\n";



    // calc
    for (int r = 0; r < total_rows; ++r)
    {
        // print all rule bits first //jointSums[MOVE_COL] = fullTT[r][N_rules + N_dirs + 1]; for [Rule Joint Table with Updates]]
        for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
        {
            showTTinfo << std::setprecision(1) << fullTT[r][ruleIdx] << "\t"; // so if a rule is active (1.0) or inactive (0.0)

        }

        
        for (int dirCol = 0; dirCol < MOVE_COL; ++dirCol) // +1 for the backtrack probs
        {


            double pDir = fullTT[r][N_rules + dirCol];
            if (pDir < 0)
            {
                showTTinfo << "_\t"; // not showing 0 or inactive directional probs
                continue;
            }

            // adding the directional prob to joint_p
            // next we multiply it with the priors (of rules based on their active/ inactive status)
            double joint_p = pDir; 

            for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
            {   
                if(count_pri++ < N_rules)
                    std::cout << "\n\t[testWitBaye.h] prev.prior: " << array_priors[ruleIdx]; // bfr updating priors
                

                double prior = array_priors[ruleIdx]; // ruleIdx only shows 0 to N_rules, using the array gives me the ruleID

                if(count_pri <= N_rules)
                    std::cout << " R[ruleIndx] " << activeRows[ruleIdx] << ": " << array_priors[ruleIdx];
                if (fullTT[r][ruleIdx] == 1.0)
                {
                    joint_p *= prior;    
                }
                else
                {    
                    joint_p *= (1.0 - prior);  // inactive 
                    
                }

            }

            //showTTinfo << std::fixed << std::setprecision(4) << joint_p << "\t";
            showTTinfo << std::fixed << std::setprecision(precision_adjust) << joint_p << "\t"; // this adds to all active dirs + Jd[Back]
            jointSums[dirCol] += joint_p;
            //jointSums[6] += joint_p_bck; // for J_d6


            // add to per-rule sum if rule is active
            for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
            {
                if (fullTT[r][ruleIdx] == 1.0)
                {
                    ruleJointSums[ruleIdx][dirCol] += joint_p;

                }
            }
        }

        // add something here to re-calc Jd[Back]

        
    
    // re-calc Jd[Back] raw term (the (1 - S) part) 
    double raw_back = fullTT[r][N_rules + MOVE_COL + 1];

    // Safety: if raw_back is weird or uninitialized, clamp it
    if (raw_back < 0.0) raw_back = 0.0;
    if (raw_back > 1.0) raw_back = 1.0;

    // compute joint probability for Jd[Back]
    double jd_back = raw_back;

    // multiply by priors for all rules (same pattern as Jd0..Jd5)
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
    {
        if (fullTT[r][ruleIdx] == 1.0)
            jd_back *= array_priors[ruleIdx];
        else
            jd_back *= (1.0 - array_priors[ruleIdx]);
    }

    // print Jd[Back] // ignore <0.0 
    if (jd_back > 0.0)
        showTTinfo << std::setprecision(precision_adjust) << jd_back << "\t";
    else    
        showTTinfo << "_\t";



    jointSums[MOVE_COL] += jd_back; // for Jd_6 // for [Sum of all directional joints] 

    // ---- accumulate per-rule sums ----
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
    {
        if (fullTT[r][ruleIdx] == 1.0)
        {
            // for Rule x directional SUMS
            ruleJointSums[ruleIdx][MOVE_COL] += jd_back;
        }
    }




        showTTinfo << "\n";


    }

    


    count_pri = 0;
    showTTinfo << "--------------------\n";

    // printing
    // use the sum of joints per dir // removed redundant outputs
    showTTinfo << "\n[Sum of all directional joints]\n";
    for (int dirCol = 0; dirCol < MOVE_COL + 1; ++dirCol)
    {
        if(jointSums[dirCol] != 0.0)
            showTTinfo << "J_d" << dirCol << " : " << std::fixed << std::setprecision(precision_adjust)
                << jointSums[dirCol] << "\t";
        else 
            showTTinfo << "J_d" << dirCol << " : " << std::fixed << std::setprecision(1)
                << jointSums[dirCol] << "\t";

        if(dirCol == 2)
                showTTinfo << "\n"; // new line for half the dirs                  
    }








    showTTinfo << "\n"; 

  




    // printing
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
    {
        showTTinfo << "\nRule " << activeRows[ruleIdx] << " directional SUMS:\n";
        for (int dirCol = 0; dirCol < MOVE_COL + 1; ++dirCol)
        {
            if(ruleJointSums[ruleIdx][dirCol] != 0.0)
                showTTinfo << "Dir" << dirCol << " : " << std::fixed << std::setprecision(precision_adjust)
                    << ruleJointSums[ruleIdx][dirCol] << "\t";
            else    
                showTTinfo << "Dir" << dirCol << " : " << std::fixed << std::setprecision(1)
                    << ruleJointSums[ruleIdx][dirCol] << "\t";
                
            if(dirCol == 2)
                showTTinfo << "\n"; // to make the printed output look less cluttered 
        }
        showTTinfo << "\n";
    }



    //std::vector<std::vector<double>> ruleUpdates(N, std::vector<double>(MOVE_COL, 0.0));
    // CHECK FOR 1.00 OR WEIRD 0.0 UPDATES 
    // after computing ruleJointSums and jointSums
    
    // calc Updates
    // store updates using the actual rule ID
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
    {
        int ridx = activeRows[ruleIdx]; // map local index to actual rule ID
        for (int dirCol = 0; dirCol < MOVE_COL + 1; ++dirCol)
        {
            if (jointSums[dirCol] > 0.0)
            {
               // ruleUpdates[ridx][dirCol] = ruleJointSums[ruleIdx][dirCol] / jointSums[dirCol]; // uncomment
            }
            else
            {
              //  ruleUpdates[ridx][dirCol] = 0.0; // avoid division by 0 // uncomment
            }
        }
    }

    showTTinfo << "\n[Rule UPDATES per dir] can't be <0.05\n";

    // print headers using actual rule IDs
    for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
        showTTinfo << "\tR" << activeRows[ruleIdx];
    showTTinfo << "\n";

    // print values using actual rule IDs
    for (int dirCol = 0; dirCol < MOVE_COL + 1; ++dirCol)
    {
        showTTinfo << "d" << dirCol << " :\t";
        for (int ruleIdx = 0; ruleIdx < N_rules; ++ruleIdx)
        {

            int ridx = activeRows[ruleIdx]; // map local index to rule ID

            // uncomment
            // if(ruleUpdates[ridx][dirCol] < 0.05 ) // this [0.05] is to adjust low updates [0.0] // doesn't have anything to do with slips
            //     ruleUpdates[ridx][dirCol] = 0.05;
            // if(ruleUpdates[ridx][dirCol] > 0.98) // likewise, this is for high updates [1.0]
            //     ruleUpdates[ridx][dirCol] = 0.98;


            // showTTinfo << std::fixed << std::setprecision(precision_adjust)
            //         << ruleUpdates[ridx][dirCol] << "\t";

            // //  checking
            // // CHECKING OCT21.2
            // update_rule_0 = ruleUpdates[0][0]; // only checking the upd for rule 0 for when it goes up [0]
        }
        showTTinfo << "\n";

    }

    // to check the calculations 
    std::cout << showTTinfo.str();
    std::cout << "\n[testWitBaye.h.h] array_priors:\n";

    for (int i = 0; i < 6; ++i) {
        std::cout << "PriR[" << i << "] = " << array_priors[i] << "\n";
    }




}









