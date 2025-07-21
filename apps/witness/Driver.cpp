// driver.cpp 19:14 Jul 15

//Driver.cpp works Jul 07


/*
 *  $Id: sample.cpp
 *  hog2
 *
 *  Created by Nathan Sturtevant on 5/31/05.
 *  Modified by Nathan Sturtevant on 02/29/20.
 *
 * This file is part of HOG2. See https://github.com/nathansttt/hog2 for licensing information.
 *
 */

#include <cstring>
#include "Common.h"
#include "Driver.h"
#include "Timer.h"
#include <deque>
#include "Witness.h"
#include "Combinations.h"
#include "FileUtil.h"
#include "SVGUtil.h"
#include <thread>
#include <mutex>
std::mutex lock;


//comment

//irrules start

// for the following two to work I edited the Makefile // DO NOT add these to pull req  
#include "WitnessInferenceRule.h" //don't use local path //comment
#include "PuzzleInferenceRule.h"

#include "sstream"
#include "set"
#include <map>



//

//May 2025

enum class StepType { FOLLOW, DEVIATE, NORMAL };

struct StepInfo {
    int action;       // 0 = Up, 1 = Down, etc.
    int ruleID;       // -1 if no rule applied
    StepType type;    // What kind of step this was
};
std::vector<StepInfo> pathLog;



//July 2025
int prev_dir = -9;
int new_dir = -9;

std::vector<std::string> ruleBuffer; //rules showing up in completed paths, valid or invalid


// for player modelling updates // helper table
struct RuleEntry {
    int x, y;
    int ruleID;
    int activeDirsCount;
    std::map<int, float> dirProbs; // 0=Up, 1=Down, L, R, inactive = -1
    int dirTaken = -1;
    int nextX = -1, nextY = -1;
};

std::vector<RuleEntry> modelTable; // do not use 'ruleTable'

const int NUM_R = 6;     // rules 
const int NUM_S = 2;     // stats aka prior, posteri, activeDirs, ... Follow, Deviate, ..

//std::vector<std::vector<int>> model_T(NUM_R, std::vector<int>(NUM_S, 0));

std::vector<std::vector<float>> model_T(NUM_R, std::vector<float>(NUM_S, -1.0f));



void Del_Fill_model_T(std::vector<std::vector<int>> &table) {
    for (int r = 0; r < table.size(); ++r) {
        for (int c = 0; c < table[r].size(); ++c) {
            table[r][c] = 1;
        }
    }
}




std::vector<RuleEntry> tempBatch;

int dirStrToInt(const std::string& dir) {
    if (dir == "Up") return 0;
    if (dir == "Down") return 1;
    if (dir == "Left") return 2;
    if (dir == "Right") return 3;
    if (dir == "Start") return 4;
    if (dir == "End") return 5;
    return -1;
}

void flushBatch() {
    modelTable.insert(modelTable.end(), tempBatch.begin(), tempBatch.end());
    tempBatch.clear();
}

void parseLogFile(const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;

    std::regex ruleLineRegex(R"(\[([0-9\-]+),\s*([0-9\-]+)\]\[Rule\]\s*(\d+):\s*(.*))");

    while (std::getline(infile, line)) {
        if (line.find("[Inv]") != std::string::npos || line.find("[Sol]") != std::string::npos) {
            flushBatch();
            continue;
        }

        std::smatch match;
        if (std::regex_search(line, match, ruleLineRegex)) {
            RuleEntry entry;
            entry.x = std::stoi(match[1]);
            entry.y = std::stoi(match[2]);
            entry.ruleID = std::stoi(match[3]);

            std::string rest = match[4];
            std::istringstream ss(rest);
            std::string token;
            while (ss >> token) {
                size_t eqPos = token.find('=');
                if (eqPos != std::string::npos) {
                    std::string dir = token.substr(0, eqPos);
                    float prob = std::stof(token.substr(eqPos + 1));
                    int dirIndex = dirStrToInt(dir);
                    if (dirIndex != -1) {
                        entry.dirProbs[dirIndex] = prob;
                    }
                }
            }
            entry.activeDirsCount = entry.dirProbs.size();
            tempBatch.push_back(entry);
        }
    }

    // final flush if needed
    flushBatch();
}






#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>
#include <string>
#include <map>
#include <iomanip>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>





float bayesianUpdate(float prior, float likelihood, int numDirs) {
    float denom = (likelihood * prior) + ((1.0f / numDirs) * (1 - prior));
    return denom > 0 ? (likelihood * prior) / denom : 0;
}



// safely del
// remove leading/trailing whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    return (first == std::string::npos || last == std::string::npos) ? "" : str.substr(first, last - first + 1);
}


// safely delete
// this is just for me to check 
// doesn't consider the next move for computing probabs - just wanted to test first - considers the current direction
void parse_rules(const std::string& logFilePath, const std::string& outFilePath = "model_res.txt", float prior = 0.2f) {
    //std::cout << "[INFO] Parsing started...\n";
    std::ifstream infile(logFilePath);
    std::ofstream outfile(outFilePath);

    if (!infile || !outfile) {
        //std::cerr << "[ERROR] Failed to open files.\n";
        return;
    }

    std::string line;
    std::string lastDir = "";
    std::pair<int, int> lastPos = {-1, -1};
    std::pair<int, int> currPos;

    while (std::getline(infile, line)) {
        // Trim line
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // Movement line
        if (line.find("] Rt") != std::string::npos ||
            line.find("] Lt") != std::string::npos ||
            line.find("] Up") != std::string::npos ||
            line.find("] Dn") != std::string::npos) {
            
            std::istringstream ss(line);
            char dummy;
            int x, y;
            std::string dir;
            ss >> dummy >> x >> dummy >> y >> dummy >> dir;

            lastPos = currPos;
            currPos = {x, y};
            lastDir = dir;

            outfile << "[Move] " << line << "\n";
        }

        // Rule line
        else if (line.find("[Rule]") != std::string::npos) {
            size_t ruleStart = line.find("[Rule]");
            size_t colonPos = line.find(':', ruleStart);
            if (ruleStart == std::string::npos || colonPos == std::string::npos) continue;

            int ruleID = std::stoi(line.substr(ruleStart + 7, colonPos - ruleStart - 7));
            std::istringstream ss(line.substr(colonPos + 1));
            std::string token;
            std::map<std::string, float> dirMap;

            while (ss >> token) {
                size_t eq = token.find('=');
                if (eq != std::string::npos) {
                    std::string dir = token.substr(0, eq);
                    float val = std::stof(token.substr(eq + 1));
                    dirMap[dir] = val;
                }
            }

            if (!lastDir.empty()) {
                std::string shortDir = lastDir.substr(0, 2); // e.g., Rt
                float likelihood = dirMap.count(shortDir) ? dirMap[shortDir] : 0.0f;
                int numDirs = dirMap.size();

                float posterior = (likelihood * prior) / (likelihood * prior + ((1.0f / numDirs) * (1 - prior)));

                outfile << "[Bayes] Rule " << ruleID
                        << " | Pos: [" << currPos.first << ", " << currPos.second << "]"
                        << " | Dir: " << shortDir
                        << " | Likelihood: " << likelihood
                        << " | Prior: " << prior
                        << " | Posterior: " << std::fixed << std::setprecision(3) << posterior << "\n";
            }
        }
    }

    outfile << "[End of parsing]\n";
    //std::cout << "[INFO] Results written to " << outFilePath << "\n";
}












//May 2025



//mid- global

// Constants for the number of rules and actions
const int NUM_RULES = 5; // Number of rules (0, 1, 2, ...)
const int NUM_ACTIONS = kWitnessActionCount; // Actions: Left, Right, Up, Down

int violationCount = 0; //stand in for percentages of rules learned

//comment //global
//std::vector< std::vector<std::string> > ruleTable(NUM_RULES, std::vector<std::string>(NUM_ACTIONS, "  ? ")); //unknown

bool tableUpdated = true; // I'm keeping it true at all times for now

int NUM_COLS  = 2; //like must, cant, unknown - last one isn't needed
//std::vector<std::vector<int>> mathTable(NUM_COLS, std::vector<int>(NUM_RULES));


//


//void TestRules(Witness<4, 4> &puzzle, WitnessState<4, 4> &state, const std::vector<WitnessAction> &actions);

//std::vector<SolutionTreeNode> solutionTree;

//bool runTestRules = false; 


//global //comment
int mustTakeSkipCounter = 0;
//#include <fstream> // Include this for file handling

//file stream to keep it open during execution
std::ofstream logFile("mouse.txt", std::ios::trunc); // Open in trunc mode to clear file
std::ofstream logFile_two("path_taken.txt", std::ios::trunc); // all visited nodes //this logs all the stuff multiple times dummy, plus you have this process elsewhere already
std::ofstream logFile_three("time_m.txt", std::ios::trunc);
std::ofstream logFile_pathAll("pathAll.txt", std::ios::trunc);
std::ofstream logFile_pathSol("pathSolved.txt", std::ios::trunc);

std::ofstream logFile_rule("r.txt", std::ios::trunc);
std::ofstream logFile_ruleAll("ra.txt", std::ios::trunc); // to track all nodes too // basically the outstream


//std::map<std::pair<int, int>, std::string> lastPrintedRuleData;
std::ofstream globalLogFile;

void InitLogFile(const std::string& data) {
	globalLogFile.open("rulesss.txt", std::ios::app); // reset file
	globalLogFile << data;
	globalLogFile.close();
}




std::vector<std::pair<int, int>> nodes; // vector to store nodes (visited??)
int mustTakeViolationCount = 0;
int cantTakeViolationCount = 0;
//std::ostringstream viewportStream;

//arcirules end


std::vector<SolutionTreeNode> solutionTree; //need the global solutionTree

//comment [NEW]

int RULE_ROW = 6; // make this dynamic

int MOVE_COL = kWitnessActionCount;
int RULE_COL = 4; //violation, correct move, percentage based on occurence or something- so 3 cols //+1 for perc of skill

//working vectors
std::vector < std::vector <std::string>> MoveTable (RULE_ROW, std::vector<std::string>(MOVE_COL,"_"));
std::vector < std::vector <int>> RuleTable (RULE_ROW, std::vector<int>(RULE_COL,0));

//std::vector < std::vector <int>> MoveTable (RULE_ROW, std::vector<int>(MOVE_COL,0));
//comment [NEW END]


//global table


#include <vector>
#include <string>
#include "Graphics.h" // Replace with the actual header file for your Graphics namespace


//working here too

//others

//global //workingdir //useless del
int prev_directionMoved = -1; //int directionMoved is the curr and this is bfr the curr
std::string prev_directionLabel = "U";

std::vector<std::string> path_taken; //full path, even backtracking


int mustTakeLogger = -1 ; //if must take is violated, don't count any cannot takes after that move, all moves are auto cant_take //1 is active so dont log, -1 is not active so log
int ruleAPLogger = -1; //if violated with must_cross then we use these TWO vars to ignore future cant_takes

//




//print rule table too


// for collecting all the learning rates for the rules
// std::vector<std::vector<int>> LearningTable;
//std::vector<std::vector<float>> LearningTable(6, std::vector<float>(4, 0));
std::vector<std::vector<float>> LearningTable(6, std::vector<float>(4, 0.0f));


// initial values for the Learning part for the rules
float r1_learn = .5;
float r2_learn = 0.4; //setting at random for now // [working] gotta set up the different cases and how to treat them

float r4_learn = .3;
float r5_learn = 0.15;
float r6_learn = 0.10;
int switch_rules = 5;




#include <vector>
#include <string>






#include <iostream>
#include <vector>
#include <string>
#include <bitset>
#include <iomanip>

// Helper: map direction code to label
std::string DirName(int code) {
    switch (code) {
        case 0: return "L";
        case 1: return "R";
        case 2: return "U";
        case 3: return "D";
        default: return "?";
    }
}


// Use this to draw the model - player model 
void PrintRuleTable(const std::vector<std::vector<int>>& table, //[NOW using] not using this table - using RuleTable instead
                Graphics::Display &d, 
                const Graphics::point& startPoint, 
                double fontSize, 
                double cellWidth, 
                double cellHeight,
				double timer,
				int T_row,
				int T_col) 
{

	/*
    //start coor
    double x = startPoint.x;
    double y = startPoint.y;

    //we're printing table rows from bottom to top btw
    for (int row = RuleTable.size() - 1; row >= 0; --row) { 
        double cellY = y - ((RuleTable.size() - row) * cellHeight); // reverse row order

        for (int col = -1; col < static_cast<int>(RuleTable[0].size()); ++col) { 

			int corr = RuleTable[row][0];  // pos occurrences
			int incorr = RuleTable[row][1]; // neg 
			int total = corr + incorr; //why not just use [row][2]??

			if (total > 0) {
				RuleTable[row][2] = total;  // Store total occurrences
				RuleTable[row][3] = (corr * 100) / (total); // store percentage

				LearningTable [row][1] = RuleTable[row][3] / 100.0f; //updating transition along with the "perc"
				//	LearningTable[0][0] = (prev_L + (1 - prev_L) * p_T) ; //make sure it doesn't exceed 1.0?? 
				//		prev_L is learningTable[0][0] for rule 1 at r0,c0
				
				// too messy
				//LearningTable [row][0] = LearningTable[row][0] + (1-LearningTable[row][0]) * LearningTable [row][1];
				float p_T = LearningTable[row][1]; // Transition probability (already 0-1 after fix)
				float prev_L = LearningTable[row][0]; // Previous learning probability

				LearningTable[row][0] = prev_L + (1 - prev_L) * p_T;
				//LearningTable[row][0] = 5; //why is this not working??

			}
			
			//COMMENT OUT THIS BLOCK

			if (LearningTable[row][1] != 0.0f) {  // Only update if transition probability is nonzero
				float p_T = LearningTable[row][1];  // Transition probability
				float prev_L = LearningTable[row][0];  // Previous learning probability

				LearningTable[row][0] = prev_L + (1 - prev_L) * p_T;  // Apply learning update

				std::cout << "update LearningTable[" << row << "][0]: " << LearningTable[row][0] << std::endl;
			}
			
			// COMMENT OUT THIS BLOCK



            double cellX = x + (col + 1) * cellWidth; // change fr row headers

            if (col == -1) {
                //(rule names)
                d.DrawText(GetRuleNameByID(row).c_str(), {x, cellY}, Colors::orange, fontSize, 0);
            } 
            else {
                //content
                int value = RuleTable[row][col];
                d.DrawText(std::to_string(value).c_str(), {cellX, cellY}, Colors::black, fontSize, 0);
            }
        }
    }

    //show col headers AFTER all the rows
    double headerY = y - ((RuleTable.size() + 1) * cellHeight); //slightly above the first row
    const std::vector<std::string> colHeaders = {"Fol.", "Dev.", "Occur", "Perc", "Guess"};

    for (int col = 0; col < static_cast<int>(RuleTable[0].size()); ++col) {
        double cellX = x + (col + 1) * cellWidth; 
        std::string colHeader = col < colHeaders.size() ? colHeaders[col] : "Unknown";
        d.DrawText(colHeader.c_str(), {cellX, headerY}, Colors::orange, fontSize, 0);
    }


	double learningTableX = x + (RuleTable[0].size() + 1) * cellWidth;
	//PrintLearningTable(LearningTable, d, {learningTableX, y}, fontSize, cellWidth, cellHeight, timer, 5, 3);

*/
 //std::cout << "\n\nN/A"; //filler line anyway, it's been a while since I touched this code yikes


	// d.DrawLine({-0.75,0}, {-0.75,0.15}, 0.01, Colors::black);

}

//print move table


//std::vector <int> APcolumns (4, -1); //4 cols, UDLR (ignoring start and end)
static std::vector <int> APcolumns_main = {0, 1, 2, 3}; //no ini


//remove Timer, T_row and T_col


// Use this to draw the model - player model 


void PrintTable_2(const std::vector<std::vector<float>>& table,
                  Graphics::Display &d,
                  const Graphics::point& startPoint,
                  double fontSize,
                  double cellWidth,
                  double cellHeight) 
{
    double x = startPoint.x;
    double y = startPoint.y;

	

    x = -0.75;
    y = 0.75;

	std::string st_msg = "[WIP] Ignore table. Should show the player model.";

	d.DrawText(st_msg.c_str(), 
                           {static_cast<float>(x), static_cast<float>(y)}, 
                           Colors::darkred, fontSize, 0);

    // Draw rows (rules)
    for (int row = table.size() - 1; row >= 0; --row) {
        double cellY = y - ((table.size() - row) * cellHeight);

        for (int col = -1; col < static_cast<int>(table[0].size()); ++col) {
            double cellX = x + (col + 1) * cellWidth;

            if (col == -1) {
                // Rule name (row header)
                d.DrawText(GetRuleNameByID(row).c_str(), 
                           {static_cast<float>(x), static_cast<float>(cellY)}, 
                           Colors::orange, fontSize, 0);
            } else {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << table[row][col];
                std::string valueStr = oss.str();

                d.DrawText(valueStr.c_str(), 
                           {static_cast<float>(cellX), static_cast<float>(cellY)}, 
                           Colors::black, fontSize, 0);
            }
        }
    }

    // Draw column headers
    double headerY = y - ((table.size() + 1) * cellHeight);
    //const std::vector<std::string> colHeaders = {"Pri", "Pos", "Dir", "Up_", "Dwn", "Lft", "Rgt"};
    const std::vector<std::string> colHeaders = {"Prior", "Posterior"};

    for (int col = 0; col < static_cast<int>(table[0].size()); ++col) {
        double cellX = x + (col + 1) * cellWidth;

        d.DrawText(colHeaders[col].c_str(), 
                   {static_cast<float>(cellX), static_cast<float>(headerY)}, 
                   Colors::orange, fontSize, 0);
    }
}



void PrintTable_2_old(const std::vector<std::vector<float>>& table,
                  Graphics::Display &d,
                  const Graphics::point& startPoint,
                  double fontSize,
                  double cellWidth,
                  double cellHeight) 
{

	// d.DrawLine({-0.75,0}, {-0.75,0.15}, 0.01, Colors::black);
    double x = startPoint.x;
    double y = startPoint.y;

    x = -0.75;
    y = 0.75;

    // Draw rows (rules)
    for (int row = table.size() - 1; row >= 0; --row) {

		
        double cellY = y - ((table.size() - row) * cellHeight);

        for (int col = -1; col < static_cast<int>(table[0].size()); ++col) {
            double cellX = x + (col + 1) * cellWidth;

            if (col == -1) {
                // Rule name (row header)
                d.DrawText(GetRuleNameByID(row).c_str(), {static_cast<float>(x), static_cast<float>(cellY)}, Colors::orange, fontSize, 0);
            } else {
                std::string valueStr;
                if (col == 3 || col == 4) {
                    // Format percentage & guess columns as decimals (e.g., 47.0%)
                    float value = table[row][col] / 100.0f;
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << value;
                    valueStr = oss.str();
                } else {
                    valueStr = std::to_string(table[row][col]);
                }
                d.DrawText(valueStr.c_str(), { static_cast<float>(cellX), static_cast<float>(cellY)}, Colors::black, fontSize, 0);
            }
        }
			
    }

    // Column headers
    double headerY = y - ((table.size() + 1) * cellHeight);
    const std::vector<std::string> colHeaders = {"Pri", "Pos", "Dir", "Up_", "Dwn", "Lft", "Rgt"};

	
    for (int col = 0; col < static_cast<int>(table[0].size()); ++col) {
        double cellX = x + (col + 1) * cellWidth;
        d.DrawText(colHeaders[col].c_str(), {static_cast<float>(cellX), static_cast<float>(headerY)}, Colors::orange, fontSize, 0);
    }
		

}


// from here for Julyx


// dec first

void PrintModelTable(const std::vector<RuleEntry>& modelTable,
                     Graphics::Display &d,
                     const Graphics::point& startPoint,
                     double fontSize,
                     double cellWidth,
                     double cellHeight);

#include <fstream>
#include <sstream>
#include <regex>

void parseLogFileToModelTable(const std::string& filename, std::vector<RuleEntry>& modelTable) {
    std::ifstream infile(filename);
    std::string line;
    std::regex ruleRegex(R"(\[(\d+), (\d+)\]\[Rule\] (\d+): (.+))");

    while (std::getline(infile, line)) {
        std::smatch match;
        if (std::regex_match(line, match, ruleRegex)) {
            RuleEntry entry;
            entry.x = std::stoi(match[1]);
            entry.y = std::stoi(match[2]);
            entry.ruleID = std::stoi(match[3]);

            // Parse direction=probability
            std::istringstream dirStream(match[4]);
            std::string token;
            entry.activeDirsCount = 0;

            while (dirStream >> token) {
                size_t eq = token.find('=');
                if (eq == std::string::npos) continue;

                std::string dirStr = token.substr(0, eq);
                float prob = std::stof(token.substr(eq + 1));

                int dir = -1;
                if (dirStr == "Up") dir = 0;
                else if (dirStr == "Down") dir = 1;
                else if (dirStr == "Left") dir = 2;
                else if (dirStr == "Right") dir = 3;
                else if (dirStr == "Start") dir = 4;
                else if (dirStr == "End") dir = 5;

                if (dir != -1) {
                    entry.dirProbs[dir] = prob;
                    entry.activeDirsCount++;
                }
            }

            modelTable.push_back(entry);
        }

        // Reset if [Sol] or [Inv] seen (optional)
        if (line.find("[Sol]") != std::string::npos || line.find("[Inv]") != std::string::npos) {
            modelTable.clear(); // comment this out if you want to keep all entries
        }
    }
}




void PrintTable(const std::vector<std::vector<std::string>>& table, 
                Graphics::Display &d, 
                const Graphics::point& startPoint, 
                double fontSize, 
                double cellWidth, 
                double cellHeight,
				double timer,
				int T_row,
				int T_col,
				const std::vector<int>& possibleDirs,
			int cX, int cY) 
{
    //start here
	double x = startPoint.x;
	double y = startPoint.y;

	



	//col headers (printed LAST)
	const std::vector<std::string> colHeaders = {"Up", "Down", "Left", "Right", "Start", "End"};


	std::vector<int> activeRows; // for the probalility table


	//rows from bottom to top
	for (int row = table.size() - 1; row >= 0; --row) { 
		double cellY = y - ((table.size() - row) * cellHeight); //rev row order

		for (int col = -1; col < static_cast<int>(table[0].size()); ++col) { 
			double cellX = x + (col + 1) * cellWidth; //odfset for row headers


			if (col == -1) {
				//rules
				d.DrawText(GetRuleNameByID(row).c_str(), {static_cast<float>(x), static_cast<float>(cellY)}, Colors::blue, fontSize, 0);
			} 
			else {

				float shift_icons = fontSize / 8;
				//vals
				if ( (table[row][col]) == "M" )
				{
					//void Graphics::Display::FillCircle(Graphics::rect r, rgbColor c)
					//d.FillCircle({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::darkgreen); //shift neg cus that's how the grid works

					d.FillCircle({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::green); //shift neg cus that's how the grid works


					if (std::find(activeRows.begin(), activeRows.end(), row) == activeRows.end()) {
						activeRows.push_back(row);
					}

				}

				else if(table[row][col]=="S") //should_take
				{
					//this isn't quite "standard" for the ATP rule gives us cant_take but every other non_c_t direction might not be possible or valid
					//so let's keep the shoudl take stuff away for along the path for now
					//d.FillSquare({cellX+shift_icons+shift_icons, cellY-shift_icons}, fontSize/1.5, Colors::bluegreen);

				}
				else if ( (table[row][col]) == "C" )
				{
					//for when we have AP - rule 5
					if(row == 4)
					{
						//std::cout<< "\t\tadded AP col: "<<col; //see if we're adding the right cols 
						//APcolumns.emplace_back(col);
						//d.FillSquare({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::orange);
						d.FillSquare({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::red);

						if (std::find(activeRows.begin(), activeRows.end(), row) == activeRows.end()) {
							activeRows.push_back(row);
						}
					}

					//normal case
					else
					{
						//d.FillSquare({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::red);
						d.FillSquare({static_cast<float>(cellX+shift_icons+shift_icons), static_cast<float>(cellY-shift_icons)}, fontSize/1.5, Colors::red);
						//d.DrawText((table[row][col]).c_str(), {cellX, cellY}, Colors::red, fontSize, 0); //only show red C

						if (std::find(activeRows.begin(), activeRows.end(), row) == activeRows.end()) {
							activeRows.push_back(row);
						}
					}
				}
				else 
				{
					d.DrawText((table[row][col]).c_str(), {static_cast<float>(cellX+shift_icons), static_cast<float>(cellY)}, Colors::black, fontSize, 0);
					//d.FillNGon({cellX+shift_icons+shift_icons, cellY-shift_icons}, fontSize/1.1, 6, 0, Colors::gray); //to see how to "centre" icons
					

				}
					
			}
		}




		}

		//print col headers (LAST, so they appear on top)
		double headerY = y - ((table.size() + 1) * cellHeight); //slightly above the first row
		for (int col = 0; col < static_cast<int>(table[0].size()); ++col) {
			double cellX = x + (col + 1) * cellWidth; //offset row headers
			d.DrawText(colHeaders[col].c_str(), {static_cast<float>(cellX), static_cast<float>(headerY)}, Colors::blue, fontSize, 0);
		}


		//we place RuleTable below the MoveTable
		double ruleTableX = x; // Align with MoveTable
		double ruleTableY = y - (MoveTable.size() + 1) * cellHeight; // space below MoveTable

		//PrintRuleTable(RuleTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, RULE_COL);
		PrintRuleTable(RuleTable, d, {static_cast<float>(ruleTableX), static_cast<float>(-ruleTableY)}, fontSize, cellWidth, cellHeight, timer, T_row, T_col);



	//std::cout << "\n ---------- [Assume we only know one rule at a time\n The combinations are just intersections of available directions common to the active known rules \nActive Rule probabilities at curr node:\n";

	const char* dirNames[] = {"Up", "Down", "Left", "Right", "Start", "End"}; // add dirs start and end otherwise the output looks odd at s/e


    std::vector<double> minProbs(possibleDirs.size(), 1.0); // initialize to 1.0 (max)


	static std::set<std::string> printed_rule_lines; // remember what I've already printed

	//static std::map<std::string, std::string> latest_rule_lines;


	


	// reset 
	static int lastCX = -1, lastCY = -1;
	if (cX != lastCX || cY != lastCY) {
		printed_rule_lines.clear();
		lastCX = cX;
		lastCY = cY;
	}



	// Loop over active rows (rules)
	for (int activeRow : activeRows) 
	{

		std::ostringstream ruleOut; //ruleOutput //for printing NEW lines/ different rules/ new node exxentially

		std::string ruleKey = "[Rule] " + std::to_string(activeRow + 1);  // we adjust to match [Rule] 5 format


		std::vector<double> probs(possibleDirs.size(), 0.0); // probabs from here

		double sum = 0.0;
		int counter = possibleDirs.size();
		int initial_counter = counter;
		int cant_counter = 0;

		// first we assign base probs to non-"M" dirs, only for active dirs
		for (size_t i = 0; i < possibleDirs.size(); ++i) {
			int dir = possibleDirs[i];
			if (table[activeRow][dir] != "M") {
				probs[i] = 0.06; // need to add to dynamic table - not model table // just checking

				//model_T[activeRow][4 + dir] = probs[i];

				sum += 0.06;
				counter -= 1;

				if (table[activeRow][dir] == "C")
				{
					cant_counter += 1;
				}


				if (probs[i] < minProbs[i]) {
					minProbs[i] = probs[i];
				}

			}
		}
		int counter_M = 0;
		// if applicable - then fill remaining "M" dirs with equal share of leftover probab using counter
		if (counter > 0) {
			double fill_val = (1.0 - sum) / counter;
			for (size_t i = 0; i < possibleDirs.size(); ++i) {
				int dir = possibleDirs[i];
				if (table[activeRow][dir] == "M") {
					probs[i] = fill_val;


					//model_T[activeRow][4 + dir] = probs[i];
					counter_M += 1;


					if (probs[i] < minProbs[i]) {
						minProbs[i] = probs[i];
					}
				}
			}
		}

		
		// WHY ELSE IF - remove later I dont wanna break this
		// there is no "M" and we only have "C"
		if (cant_counter > 0 && counter_M == 0) {
			int new_sum = cant_counter * 0.06;
			//ruleOut << "\t\n [sum, ini, cant]"<< new_sum << "\t" << initial_counter << "\t" << cant_counter << "\t [e]\n"; //rules show here
			double fill_val = (1.0 - cant_counter * 0.06) / (initial_counter - cant_counter);
			for (size_t i = 0; i < possibleDirs.size(); ++i) {
				int dir = possibleDirs[i];
				if (table[activeRow][dir] != "C") {
					probs[i] = fill_val;


					//model_T[activeRow][4 + dir] = probs[i];

					


					if (probs[i] < minProbs[i]) {
						minProbs[i] = probs[i];
					}
				}
			}
		}

		// no direction is valid
		/*
		if (cant_counter == initial_counter)
		{
			ruleOut << "\t\t --- all invalid\n\n"; //rules show here
			for (size_t i = 0; i < possibleDirs.size(); ++i) {
				int dir = possibleDirs[i];
				probs[i] = 0.06; //should be 0.06
				
			}
		}
		*/

		bool allWeak = true;
		for (size_t i = 0; i < minProbs.size(); ++i) {
			if (minProbs[i] > 0.06 + 1e-6) { // some stronger direction exists
				allWeak = false;
				break;
			}
		}

		//if (allWeak) {
		//	ruleOut << "  [all directions invalid]";
		//}



		

		//Julyx

		//Julyworks

		/*
		float r_prior = 0.2;
		float r_poste = -1;
		int r_dir = counter; // or possibleDirs.size
		float r_up = -1;
		float r_dn = -1;
		float r_lt = -1;
		float r_rt = -1;

		int rule_ID = activeRow;

		// Only update if this row has not been filled
		
		if (model_T[rule_ID][0] == -2) {
			model_T[rule_ID][0] = r_prior;
			model_T[rule_ID][1] = r_poste;
			model_T[rule_ID][2] = r_dir;


			for (size_t i = 0; i < possibleDirs.size(); ++i) {
				int dir = possibleDirs[i];  // dir is expected to be 0=Up, 1=Dn, 2=Lt, 3=Rt

				switch (dir) {
					case 0: r_up = probs[i]; break;
					case 1: r_dn = probs[i]; break;
					case 2: r_lt = probs[i]; break;
					case 3: r_rt = probs[i]; break;
					default: break;  // ignore start/end/etc.
				}
			}



		}
		*/


		//const char* dirNames[] = {"Up", "Down", "Left", "Right"};

		

		ruleOut << "\n\t[" << cX << ", "<<cY<<"]"; // removed odd space // fix parser as it worked with said odd space
		//ruleOut << "\n\t [" << cX << ", "<<cY<<"]";
		ruleOut << "[Rule] " << activeRow+1 << ": ";

		for (size_t i = 0; i < possibleDirs.size(); ++i) {
			int dirIdx = possibleDirs[i];

			// protect against invalid dir index
			if (dirIdx >= 0 && dirIdx < 6) {
				//std::cout <<"\ttest\t";
				ruleOut << dirNames[dirIdx] << "=" << std::fixed << std::setprecision(3) << probs[i] << "  ";

				//model_T[activeRow][dirIdx + 4] = probs[i];

			} else {
				ruleOut << "[?Dir" << dirIdx << "]=" << std::fixed << std::setprecision(3) << probs[i] << "  ";

				//model_T[activeRow][dirIdx + 4] = probs[i];
			}
		}

		// append this *after* rule info so it stays on the same line
		if (allWeak) {
			//ruleOut << "[all dirs invalid]"; //wrong
		}

		ruleOut << "\n";



		/*
		if (mySet.find(x) == mySet.end()) {
			// x is not in the set
		} // aka end() points past the last ele of the container/ set
		*/


		// remove this block
		int tog_x = -9;
		// only print if it's new
		std::string ruleLine = ruleOut.str();
		if (printed_rule_lines.find(ruleLine) == printed_rule_lines.end()) {
			std::cout << ruleLine;

			//log__F
			logFile_ruleAll << ruleLine;
			printed_rule_lines.insert(ruleLine); // remember it ig

			logFile_rule << ruleLine;
			tog_x = 2;
		}


		

		RuleEntry entry;
		entry.x = x;
		entry.y = y;
		entry.ruleID = activeRow;
		entry.activeDirsCount = 0;

		for (int dir = 0; dir < possibleDirs.size(); dir++) {
			if (possibleDirs[dir]) { // you should already know which directions are active
				float prob = probs[dir]; // or however you're storing the per-direction probability
				entry.dirProbs[dir] = prob;
				entry.activeDirsCount++;
			}
		}

		//entry.dirTaken = dirNames[dirIdx];     // if known
		entry.nextX = cX;           // if known
		entry.nextY = cY;           // if known

		modelTable.push_back(entry);




	}
	

}


//up to here Julyx





//Julmodel

void PrintModelTable(const std::vector<RuleEntry>& modelTable,
                     Graphics::Display &d,
                     const Graphics::point& startPoint,
                     double fontSize,
                     double cellWidth,
                     double cellHeight) 
{
    double x = -0.75;
    double y = 0;

	//d.DrawLine({-0.75,0}, {-0.75,0.15}, 0.01, Colors::black);

    // Column headers
    const std::vector<std::string> colHeaders = {"x", "y", "Rule", "Up", "Dn", "Lt", "Rt", "Taken", "→(x,y)"};
    for (size_t col = 0; col < colHeaders.size(); ++col) {
        double cellX = x + col * cellWidth;
        d.DrawText(colHeaders[col].c_str(), {static_cast<float>(cellX), static_cast<float>(y)}, Colors::black, fontSize, 0);
    }

    // Print each RuleEntry
    for (size_t i = 0; i < modelTable.size(); ++i) {
        const RuleEntry& entry = modelTable[i];
        double rowY = y - ((i + 1) * cellHeight);

        // Columns
        std::vector<std::string> row = {
            std::to_string(entry.x),
            std::to_string(entry.y),
            std::to_string(entry.ruleID),
            entry.dirProbs.count(0) ? std::to_string(entry.dirProbs.at(0)) : "-",
            entry.dirProbs.count(1) ? std::to_string(entry.dirProbs.at(1)) : "-",
            entry.dirProbs.count(2) ? std::to_string(entry.dirProbs.at(2)) : "-",
            entry.dirProbs.count(3) ? std::to_string(entry.dirProbs.at(3)) : "-",
            (entry.dirTaken == -1 ? "-" : std::to_string(entry.dirTaken)),
            (entry.nextX == -1 ? "-" : "(" + std::to_string(entry.nextX) + "," + std::to_string(entry.nextY) + ")")
        };

        for (size_t col = 0; col < row.size(); ++col) {
            double cellX = x + col * cellWidth;
            d.DrawText(row[col].c_str(), {static_cast<float>(cellX), static_cast<float>(rowY)}, Colors::black, fontSize, 0);
        }
    }

    // Optional grid lines
   // d.DrawLine({x, y}, {x + colHeaders.size() * cellWidth, y}, 0.002, Colors::gray); // top
}


//Julmodel



//global table end









bool recording = false;
bool parallel = false;
// 2x2 + 5 (interesting)
// 2x5 + 5 [201/200][149][137/139][82][64][50]
// 3x4 + 5 [481/471][463!][399!][374! - 373][260][150][146][126]
const int puzzleWidth = 4;
const int puzzleHeight = 4;
const int minSolutions = 5000;
const int numRequiredPieces = 4; // 5
const int numSeparationPieces = 4; // 3x4 + 6 [4]
int currBoard = 0;



//for web test

// Jul 20
std::vector<std::vector<Witness<4, 4>>> puzzleSet; // set of puzzles for 4x4 board which we will use for the lab test
// Witness<4, 4> wp44; // a 4x4 board that we'll probably change as we keep solving // we'll use iws instead
 InteractiveWitnessState<4, 4> iws4; // iws for wp44

int whichPuzzle = 0; // variable we'll use to increment the puzzle
int puzzleNum = 4; // fix at 4 if we have 4 puzzles
bool redrawBackground = true; // this too?


Witness<puzzleWidth, puzzleHeight> w;
InteractiveWitnessState<puzzleWidth, puzzleHeight> iws;
std::vector<Witness<puzzleWidth, puzzleHeight>> best;


//std::vector<uint64_t> otherbest;

void GetAllSolutions();
int CountSolutions(const Witness<puzzleWidth, puzzleHeight> &w,
				   const std::vector<WitnessState<puzzleWidth, puzzleHeight>> &allSolutions, int &len, int limit);
int CountSolutions(const Witness<puzzleWidth, puzzleHeight> &w,
				   const std::vector<WitnessState<puzzleWidth, puzzleHeight>> &allSolutions,
				   std::vector<int> &solutions,
				   const std::vector<int> &forbidden,
				   int &len, int limit);
void ExamineMustCross(int count);
void Load(uint64_t rank);
void ExamineMustCrossAndRegions(int crossCount, int regionCount);
void ExamineMustCrossAnd3Regions(int crossCount, int regionCount);
void ExamineTetris(int count);
void ExamineTriangles(int count);
void ExamineRegionsAndStars(int count);
void ParallelExamine(int count);
void GenerateUnique44();

template <int puzzleWidth, int puzzleHeight>
void GetAllSolutions(const Witness<puzzleWidth, puzzleHeight> &w, std::vector<WitnessState<puzzleWidth, puzzleHeight>> &puzzles);

int main(int argc, char* argv[])
{
//	GetAllSolutions();
	InstallHandlers();
	RunHOGGUI(argc, argv, 640, 640);
	return 0;
}

/**
 * Allows you to install any keyboard handlers needed for program interaction.
 */
void InstallHandlers()
{
	InstallKeyboardHandler(MyDisplayHandler, "Solve", "Solve current board", kAnyModifier, 'v');
	InstallKeyboardHandler(MyDisplayHandler, "Test", "Test constraints", kAnyModifier, 't');
	InstallKeyboardHandler(MyDisplayHandler, "Record", "Record a movie", kAnyModifier, 'r');
	InstallKeyboardHandler(MyDisplayHandler, "Save", "Save current puzzle as svg", kAnyModifier, 's');
	InstallKeyboardHandler(MyDisplayHandler, "Cycle Abs. Display", "Cycle which group abstraction is drawn", kAnyModifier, '\t');
	InstallKeyboardHandler(MyDisplayHandler, "Prev Board", "Jump to next found board.", kAnyModifier, '[');
	InstallKeyboardHandler(MyDisplayHandler, "Next Board", "Jump to prev found board", kAnyModifier, ']');
	InstallKeyboardHandler(MyDisplayHandler, "Prev 100 Board", "Jump to next 100 found board.", kAnyModifier, '{');
	InstallKeyboardHandler(MyDisplayHandler, "Next 100 Board", "Jump to prev 100 found board", kAnyModifier, '}');

	InstallCommandLineHandler(MyCLHandler, "-run", "-run", "Runs pre-set experiments.");
	InstallCommandLineHandler(MyCLHandler, "-test", "-test", "Basic test with MD heuristic");

	InstallWindowHandler(MyWindowHandler);
	InstallMouseClickHandler(MyClickHandler, static_cast<tMouseEventType>(kMouseMove|kMouseUp|kMouseDrag));
}

void DrawPaperLevel(int which)
{
	std::string fname = "/Users/nathanst/Pictures/SVG/witness/";
	rgbColor blueColor(0.354, 0.666, 0.824);
	switch (which)
	{
		case 0:
		{
			Graphics::Display d;
			Witness<1, 2> p;
			InteractiveWitnessState<1, 2> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(0, 1, Colors::black);
			p.SetGoal(1, 1);
			p.SetStart(0, 1);
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.currState = InteractiveWitnessState<1, 2>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 1:
		{
			Graphics::Display d;
			Witness<1, 2> p;
			InteractiveWitnessState<1, 2> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(0, 1, Colors::black);
			p.SetGoal(1, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.currState = InteractiveWitnessState<1, 2>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 2:
		{
			Graphics::Display d;
			Witness<1, 3> p;
			InteractiveWitnessState<1, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.SetGoal(1, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});
			p1.currState = InteractiveWitnessState<1, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 3:
		{
			Graphics::Display d;
			Witness<2, 2> p;
			InteractiveWitnessState<2, 2> p1;
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);
			p.SetGoal(2, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 3});
			p1.currState = InteractiveWitnessState<2, 2>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 4:
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.SetGoal(3, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 4});
			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 5:
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.SetGoal(0, 1);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({-1, 1});
			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 6:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, blueColor);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(3, 1, blueColor);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.AddSeparationConstraint(3, 2, blueColor);
			p.AddSeparationConstraint(0, 3, blueColor);
			p.AddSeparationConstraint(2, 3, Colors::black);
			p.AddSeparationConstraint(3, 3, blueColor);
			p.SetGoal(1, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({4, 0});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({1, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 7:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, blueColor);
			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.AddSeparationConstraint(3, 2, blueColor);

			p.AddSeparationConstraint(0, 3, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 3, Colors::black);
			p.AddSeparationConstraint(3, 3, blueColor);
			p.SetGoal(4, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({5, 2});


			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 8:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, blueColor);

			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(3, 2, blueColor);

			p.AddSeparationConstraint(1, 3, blueColor);
			p.AddSeparationConstraint(2, 3, Colors::black);
			p.AddSeparationConstraint(3, 3, blueColor);
			p.SetGoal(3, 0);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, -1});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 9:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, blueColor);
			p.AddSeparationConstraint(3, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.AddSeparationConstraint(3, 2, blueColor);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(1, 3, blueColor);
			p.AddSeparationConstraint(2, 3, blueColor);
			p.AddSeparationConstraint(3, 3, Colors::black);

			p.SetGoal(4, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({4, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 10:
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(2, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.SetGoal(1, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({1, 5});
			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 11:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.AddSeparationConstraint(3, 2, Colors::black);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(1, 3, Colors::black);
			p.AddSeparationConstraint(2, 3, blueColor);

			p.SetGoal(4, 0);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({4, 0});
			p1.ws.path.push_back({5, 0});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}


		// Spread curriculum
		case 12:
		{
			Graphics::Display d;
			Witness<1, 2> p;
			InteractiveWitnessState<1, 2> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(0, 1, Colors::black);
			p.SetGoal(1, 1);
			p.SetStart(0, 1);
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.currState = InteractiveWitnessState<1, 2>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 13:
		{
			Graphics::Display d;
			Witness<1, 3> p;
			InteractiveWitnessState<1, 3> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.SetGoal(1, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});
			p1.currState = InteractiveWitnessState<1, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 14:
		{
			Graphics::Display d;
			Witness<2, 2> p;
			InteractiveWitnessState<2, 2> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.SetGoal(0, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.currState = InteractiveWitnessState<2, 2>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 15:
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.SetGoal(1, 0);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({1, -1});
			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 16:
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, blueColor);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.SetGoal(2, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({2, 4});
			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 17:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, Colors::black);

			p.AddSeparationConstraint(2, 1, Colors::black);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(0, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.AddSeparationConstraint(3, 2, blueColor);

			p.AddSeparationConstraint(2, 3, blueColor);
			p.AddSeparationConstraint(3, 3, blueColor);

			p.SetGoal(4, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({4, 0});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({5, 2});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 18:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);
			p.AddSeparationConstraint(3, 1, Colors::black);

			p.AddSeparationConstraint(3, 2, Colors::black);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(1, 3, Colors::black);
			p.AddSeparationConstraint(3, 3, Colors::black);

			p.SetGoal(0, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({0, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 19:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, blueColor);
			p.AddSeparationConstraint(3, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);
			p.AddSeparationConstraint(2, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(3, 2, blueColor);

			p.AddSeparationConstraint(1, 3, Colors::black);
			p.AddSeparationConstraint(2, 3, Colors::black);

			p.SetGoal(0, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({3, 4});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({0, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 20:
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);
			p.AddSeparationConstraint(3, 1, Colors::black);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.AddSeparationConstraint(3, 2, Colors::black);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(2, 3, blueColor);
			p.AddSeparationConstraint(3, 3, blueColor);

			p.SetGoal(4, 0);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({1, 4});
			p1.ws.path.push_back({2, 4});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 0});
			p1.ws.path.push_back({5, 0});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		// TODO: finish these
		case 21: // 0 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, blueColor);
			p.AddSeparationConstraint(2, 0, blueColor);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);

			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);

			p.SetGoal(3, 2);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({4, 2});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 22: // 1 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);

			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, Colors::black);

			p.SetGoal(2, 0);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({2, -1});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 23: // 2 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(1, 0, blueColor);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(1, 2, blueColor);

			p.SetGoal(1, 3);
			p.SetStart(0, 0);

			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 24: // 3 of 8
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);
			p.AddSeparationConstraint(3, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.AddSeparationConstraint(3, 2, Colors::black);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(1, 3, Colors::black);

			p.SetGoal(0, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({4, 1});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({0, 4});
			p1.ws.path.push_back({0, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 25: // 4 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, blueColor);
			p.AddSeparationConstraint(2, 2, blueColor);
			p.SetGoal(3, 3);
			p.SetStart(0, 0);

			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({3, 0});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 4});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 26: // 5 of 8
		{
			Graphics::Display d;
			Witness<1, 3> p;
			InteractiveWitnessState<1, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(0, 2, blueColor);
			p.SetGoal(1, 3);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({1, 4});

			p1.currState = InteractiveWitnessState<1, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 27: // 6 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(1, 0, Colors::black);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(1, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);
			p.SetGoal(3, 1);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({0, 3});
			p1.ws.path.push_back({1, 3});
			p1.ws.path.push_back({2, 3});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({4, 1});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 28: // 7 of 8
		{
			Graphics::Display d;
			Witness<3, 3> p;
			InteractiveWitnessState<3, 3> p1;
			p.AddSeparationConstraint(0, 0, Colors::black);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, blueColor);

			p.AddSeparationConstraint(0, 1, blueColor);
			p.AddSeparationConstraint(1, 1, blueColor);

			p.AddSeparationConstraint(0, 2, Colors::black);
			p.AddSeparationConstraint(2, 2, Colors::black);

			p.SetGoal(3, 3);
			p.SetStart(0, 0);

			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({0, 2});
			p1.ws.path.push_back({1, 2});
			p1.ws.path.push_back({2, 2});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({3, 3});
			p1.ws.path.push_back({3, 4});

			p1.currState = InteractiveWitnessState<3, 3>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		case 29: // 8 of 8
		{
			Graphics::Display d;
			Witness<4, 4> p;
			InteractiveWitnessState<4, 4> p1;
			p.AddSeparationConstraint(0, 0, blueColor);
			p.AddSeparationConstraint(1, 0, Colors::black);
			p.AddSeparationConstraint(2, 0, blueColor);

			p.AddSeparationConstraint(0, 1, Colors::black);
			p.AddSeparationConstraint(1, 1, Colors::black);
			p.AddSeparationConstraint(2, 1, Colors::black);
			p.AddSeparationConstraint(3, 1, blueColor);

			p.AddSeparationConstraint(3, 2, Colors::black);

			p.AddSeparationConstraint(0, 3, Colors::black);
			p.AddSeparationConstraint(1, 3, Colors::black);
			p.AddSeparationConstraint(2, 3, Colors::black);
			p.AddSeparationConstraint(3, 3, Colors::black);

			p.SetGoal(4, 4);
			p.SetStart(0, 0);
			p1.ws.path.push_back({0, 0});
			p1.ws.path.push_back({0, 1});
			p1.ws.path.push_back({1, 1});
			p1.ws.path.push_back({1, 0});
			p1.ws.path.push_back({2, 0});
			p1.ws.path.push_back({2, 1});
			p1.ws.path.push_back({3, 1});
			p1.ws.path.push_back({3, 2});
			p1.ws.path.push_back({4, 2});
			p1.ws.path.push_back({4, 3});
			p1.ws.path.push_back({4, 4});
			p1.ws.path.push_back({4, 5});

			p1.currState = InteractiveWitnessState<4, 4>::kInPoint;
			p.Draw(d);
			p.Draw(d, p1);
			printf("Save to '%s'\n", (fname+std::to_string(which)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(which)+".svg").c_str(), 600, 600, 0);
			break;
		}
		default: break;
	}

}








//may 2025

// forget backtracking, we're using this path instead
void LogPathCompliance(const Witness<puzzleWidth, puzzleHeight> &w,
                       const WitnessState<puzzleWidth, puzzleHeight> &ws) {
    const auto &path = ws.path;
    if (path.size() < 2) return;

    std::ofstream log("replay_log.txt");

    // Direction sequence
    log << "Direction sequence (number and label):\n";
    for (size_t i = 0; i < path.size() - 1; ++i) {
        auto [x1, y1] = path[i];
        auto [x2, y2] = path[i + 1];
        int dir = (x2 > x1) ? 3 : (x2 < x1) ? 2 : (y2 > y1) ? 0 : 1;
        std::string label = (dir == 0) ? "Up" : (dir == 1) ? "Down" : (dir == 2) ? "Left" : "Right";
        log << dir << " (" << label << ") ";
    }
    log << "\n\n";

    // Full path
    log << "Full path:\n";
    for (auto [x, y] : path) log << "(" << x << ", " << y << ") ";
    log << "\n\n";

    // Rule compliance per step
    for (size_t i = 1; i < path.size(); ++i) {
        auto [x1, y1] = path[i - 1];
        auto [x2, y2] = path[i];

        int dir = (x2 > x1) ? 3 : (x2 < x1) ? 2 : (y2 > y1) ? 0 : 1;

        log << "Step " << i - 1 << ": (" << x1 << ", " << y1 << ") -> (" << x2 << ", " << y2 << ")  | Dir: " << dir << "\n";

        WitnessState<puzzleWidth, puzzleHeight> tempState = ws;
        tempState.path = std::vector<std::pair<int, int>>(path.begin(), path.begin() + i + 1);

        std::vector<WitnessAction> guesses;
        w.GetActions(tempState, guesses);

        log << "  Guess: [";
        for (size_t j = 0; j < guesses.size(); ++j) {
            log << static_cast<int>(guesses[j]);
            if (j < guesses.size() - 1) log << ", ";
        }
        log << "]\n";

		// for probability table
		std::vector<int> possibleDirs;
		for (const auto& action : guesses) {
			possibleDirs.push_back(static_cast<int>(action));
		}


        for (const auto &action : guesses) {
            for (const auto &[ruleID, ruleFn] : witnessInferenceRules<puzzleWidth, puzzleHeight>) {
                ActionType result = ruleFn(w, tempState, action);
                if (result == MUST_TAKE) {
                    log << "    FOLLOW (MUST_TAKE) - Rule: " << GetRuleNameByID(ruleID) << "\n";
                } else if (result == CANNOT_TAKE) {
                    log << "    DEVIATE (CANNOT_TAKE) - Rule: " << GetRuleNameByID(ruleID) << "\n";
                } else {
                    log << "    --- (UNKNOWN) - Rule: " << GetRuleNameByID(ruleID) << "\n";
                }
            }
        }
    }

    log << "Final Position: (" << path.back().first << ", " << path.back().second << ")\n";
    log.close();
}


//may 2025






static bool vp_active = true; 



void MyWindowHandler(unsigned long windowID, tWindowEventType eType)
{
	if (eType == kWindowDestroyed)
	{
		printf("Window %ld destroyed\n", windowID);
		RemoveFrameHandler(MyFrameHandler, windowID, 0);
		RemoveFrameHandler(MySecondFrameHandler, windowID, nullptr);
	}
	else if (eType == kWindowCreated)
	{
//		for (int x = 0; x < 50; x++)
//			DrawPaperLevel(x);

		printf("Window %ld created\n", windowID);
		InstallFrameHandler(MyFrameHandler, windowID, 0);
		SetNumPorts(windowID, 2); //comment
		ReinitViewports(windowID, {-1, -1, 0,1}, kScaleToSquare);
		if(vp_active == true)
		{

			AddViewport(windowID, {0, -1, 1, 1}, kScaleToSquare);
			InstallFrameHandler(MySecondFrameHandler, windowID, nullptr);
		}

		puzzleSet.resize(4); // 4 puzzles for now // Jul20

		submitTextToBuffer(" submitTextToBuffer : test - witness 3 "); // add the submit text to buffer part ?? //comments

		Witness<4, 4> tmp44w;

		// puzzle 1
		tmp44w.ClearSeparationConstraints();
		tmp44w.AddSeparationConstraint(1, 2, Colors::pink);
		tmp44w.AddSeparationConstraint(2, 2, Colors::yellow);

		puzzleSet[0].push_back(tmp44w);

		// puzzle 2
		tmp44w.ClearSeparationConstraints();
		tmp44w.AddSeparationConstraint(0, 3, Colors::green);
		tmp44w.AddSeparationConstraint(1, 1, Colors::purple);
		tmp44w.AddSeparationConstraint(1, 2, Colors::purple);

		puzzleSet[1].push_back(tmp44w);

		// puzzle 3
		tmp44w.ClearSeparationConstraints();
		tmp44w.AddSeparationConstraint(1, 2, Colors::red);
		tmp44w.AddSeparationConstraint(2, 2, Colors::black);

		puzzleSet[2].push_back(tmp44w);


		// puzzle 4
		tmp44w.ClearSeparationConstraints();
		tmp44w.AddSeparationConstraint(1, 2, Colors::pink);
		tmp44w.AddSeparationConstraint(2, 2, Colors::pink);
		tmp44w.AddSeparationConstraint(1, 1, Colors::yellow);
		tmp44w.AddSeparationConstraint(2, 1, Colors::yellow);

		puzzleSet[3].push_back(tmp44w);




		//printf("Window %ld created\n", windowID);
		//InstallFrameHandler(MyFrameHandler, windowID, 0);
		//SetNumPorts(windowID, 1);

/*
		w.SetStart(0, 0);
		w.SetGoal(3, 5);
//		w.AddCannotCrossConstraint(true, 0, 0);
		w.AddSeparationConstraint(3, 3, Colors::blue);
		w.AddSeparationConstraint(0, 3, Colors::blue);
		w.AddSeparationConstraint(1, 2, Colors::black);
		w.AddStarConstraint(3, 1, Colors::black);

*/
///*


		// board 1 
		/*
		w.AddSeparationConstraint(2, 1, Colors::pink);
		w.AddSeparationConstraint(2, 2, Colors::purple);
		w.AddSeparationConstraint(3, 1, Colors::pink);
		w.AddSeparationConstraint(3, 2, Colors::purple); 

		w.AddSeparationConstraint(2, 3, Colors::pink);
		w.AddSeparationConstraint(3, 3, Colors::pink); 
		*/

		// board 2
		///*
		w.AddSeparationConstraint(2, 1, Colors::pink);
		w.AddSeparationConstraint(2, 2, Colors::pink);
		w.AddSeparationConstraint(3, 1, Colors::yellow);
		w.AddSeparationConstraint(3, 2, Colors::yellow);
		//*/

		// board 3
		/*
		
		w.AddSeparationConstraint(2, 1, Colors::pink);
		w.AddSeparationConstraint(2, 2, Colors::purple);
		w.AddSeparationConstraint(3, 1, Colors::pink);
		w.AddSeparationConstraint(3, 2, Colors::purple); 

		w.AddSeparationConstraint(2, 3, Colors::pink);
		w.AddSeparationConstraint(3, 3, Colors::pink); 
		*/

		//w.AddStarConstraint(1, 1, Colors::pink);
		//w.AddStarConstraint(0, 1, Colors::pink);
		//w.AddStarConstraint(1, 1, Colors::pink);
		//w.AddSeparationConstraint(1, 1, Colors::pink);
		//w.AddSeparationConstraint(1, 2, Colors::yellow);
//*/

//		w.AddTriangleConstraint(0, 0, 3);
//		w.AddGoal(1, -1);
//		w.AddGoal(-1, 1);
//		w.AddGoal(puzzleWidth, 0);
//		w.AddGoal(0, puzzleHeight);
//		w.AddGoal(puzzleWidth, puzzleHeight+1);
//		w.AddTriangleConstraint(0, 1, 2);
//		w.AddTriangleConstraint(1, 0, 1);
//		ExamineMustCross(numRequiredPieces);
//		w.AddStarConstraint(0, 0, Colors::pink);
//		w.AddSeparationConstraint(0, 0, Colors::green);
//		w.AddSeparationConstraint(0, 4, Colors::white);
//		w.AddSeparationConstraint(4, 0, Colors::white);
//		w.AddSeparationConstraint(2, 2, Colors::pink);
//		w.AddSeparationConstraint(4, 4, Colors::black);
//		w.AddSeparationConstraint(2, 0, Colors::black);
//		w.AddSeparationConstraint(2, 4, Colors::black);
//		w.AddSeparationConstraint(0, 2, Colors::orange);
//		w.AddSeparationConstraint(4, 2, Colors::orange);

//		std::string s = "{\"dim\":\"3x3\",\"cc\":{\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"4;3;#385CDE\",\"3;10;#DCA700\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\"},\"mc\":\"0000000000000000000000000000000000000000\"}\"";
//		w.LoadFromHashString(s);

//		std::string s2 = "{\"dim\":\"4x4\",\"cc\":{\"1;0;#FFFFFF\",\"1;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"1;0;#FFFFFF\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\",\"0;0;#000000\"},\"mc\":\"10000000000000000000000000001100000000000000000000000000000000000\"}";
//		std::string s = "{\"dim\":\"4x4\",\"cc\":{\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"0;0;#000000\",\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"1;1077084160;#FFFFFF\",\"0;0;#000000\",\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"1;0;#000000\",\"0;0;#000000\",\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"1;0;#FFFFFF\",\"0;0;#000000\"},\"mc\":\"00000000010000010000000000000001000000000000000000000000000000000\"}";
//		s = 	"{\"dim\":\"4x4\",\"cc\":{\"0;0;#FFFFFF\",\"0;0;#FFFFFF\",\"0;0;#0000FF\",\"0;0;#0000FF\",\"0;0;#FFFFFF\",\"1;0;#FFFFFF\",\"0;0;#0000FF\",\"0;0;#0000FF\",\"0;0;#FFFFFF\",\"1;0;#000000\",\"0;0;#0000FF\",\"1;0;#0000FF\",\"0;0;#FFFFFF\",\"0;0;#0000FF\",\"0;0;#0000FF\",\"1;0;#FFFFFF\"},\"mc\":\"00000010000000000000000000000000000000000000000000000000000000000\"}";
//		s = "{\"dim\":\"4x4\",\"cc\":{\"1;0;#0000FF\",\"0;0;#000026D\",\"0;0;#0029D00\",\"2;0;#0000FF\",\"1;0;#0000FF\",\"0;0;#000000\",\"0;0;#000000\",\"1;0;#0000FF\",\"2;0;#0000FF\",\"0;0;#000000\",\"0;0;#000000\",\"1;0;#0000FF\",\"1;0;#0000FF\",\"0;0;#000000\",\"0;0;#000000\",\"1;0;#0000FF\"},\"mc\":\"10000000000010000000000000000000100000000000000000000000000000000\"}";

//		w.LoadFromHashString(s);
//		w.AddTriangleConstraint(0, 0, 1);
//		w.AddTriangleConstraint(1, 1, 2);
//		w.AddTriangleConstraint(2, 2, 3);
//		w.AddStarConstraint(3, 3, Colors::orange);
//		w.AddCannotCrossConstraint(1, 4);
//		w.AddCannotCrossConstraint(3, 1);
//		w.AddCannotCrossConstraint(false /*vert*/, 2, 1);
//		w.AddCannotCrossConstraint(true /*horiz*/, 0, 2);
		//		w.AddTetrisConstraint(1, 1, 10);
//		w.AddNegativeTetrisConstraint(1, 0, 3);
//		w.AddTetrisConstraint(2, 1, 2);
//		w.AddTetrisConstraint(1, 2, 2);
//		w.AddTetrisConstraint(2, 2, 2);
//		w.AddTetrisConstraint(1, 3, 2);
//		w.AddTetrisConstraint(2, 3, 2);
//		w.AddMustCrossConstraint(false, 2, 0);
//		w.AddMustCrossConstraint(false, 2, 4);


//		w.AddTetrisConstraint(0, 2, 2);
//		w.AddTetrisConstraint(0, 1, 1);
//		w.AddTetrisConstraint(3, 3, );
//		w.AddTetrisConstraint(0, 0, 10);
//		w.AddTetrisConstraint(3, 0, 12);
//		w.AddTetrisConstraint(3, 1, 14);
//		w.AddTetrisConstraint(2, 3, -8);
//		w.AddTetrisConstraint(2, 2, -12);
//		w.AddTetrisConstraint(1, 1, -13);
//		w.AddTetrisConstraint(1, 0, 4);
//		w.AddTetrisConstraint(1, 1, 5);
//		w.AddTetrisConstraint(1, 2, 6);
//		w.AddTetrisConstraint(1, 3, 7);
//		w.AddTetrisConstraint(2, 0, 8);
//		w.AddTetrisConstraint(2, 1, 9);
//		w.AddTetrisConstraint(2, 2, 10);
//		w.AddTetrisConstraint(2, 3, 11);
	}
}




//comment

//comment frame handler
void MyFrameHandler(unsigned long windowID, unsigned int viewport, void *)
{
	
	if(viewport != 0)
		return;
	/*
	Graphics::Display &d = GetContext(windowID)->display;
	iws.IncrementTime();
	w.Draw(d);
	w.Draw(d, iws);
	*/


	Graphics::Display &display = getCurrentContext()->display;
	if (whichPuzzle < puzzleNum) // for four puzzles as we go from 0 to 3
	{
		if (viewport == 0)
			iws.IncrementTime();
		if (redrawBackground)
		{
			display.StartBackground();
			puzzleSet[whichPuzzle][0].Draw(display); // viewport is 0
			display.EndBackground();
		}
		puzzleSet[whichPuzzle][0].Draw(display, iws);
	}
	else if (whichPuzzle == puzzleNum){
		if (redrawBackground)
		{
			display.StartBackground();
			display.FillRect({-1.0f, -1.0f, 1.0f, 1.0f}, Colors::white);
			display.EndBackground();
		}
		display.DrawText("Puzzles Completed!", {0.0f, 0.0f}, Colors::pink, 0.1f, Graphics::textAlignCenter);
	}
	if (viewport == 1) // for the table info viewport
		redrawBackground = false; // comments use this too


}



void MySecondFrameHandler(unsigned long windowID, unsigned int viewport, void *)
{
	if(viewport != 1)
		return;
	if(vp_active == false)
		return;

	Graphics::Display &disp = GetContext(windowID)->display;	
	disp.FillRect({-1, -1, 1, 1}, Colors::white);

	WitnessState<puzzleWidth, puzzleHeight> currentState = iws.ws;

	std::vector<WitnessAction> actions; //possble acts not taken or IR related acts (i.e., must_take, cant_take)
	w.GetActions(currentState, actions);

	std::vector<int> possibleDirs;
	for (const auto& action : actions) {
		possibleDirs.push_back(static_cast<int>(action));
	}

	// to draw moves table
	int RULE_ROW = witnessInferenceRules<puzzleWidth, puzzleHeight>.size(); //comment //dynamic rows 

	
	std::vector < std::vector <std::string>> MoveTable (RULE_ROW, std::vector<std::string>(MOVE_COL,".")); // placeholder, can be replaced with cant_take

	int frame_currX = -1, frame_currY = -1;

	if (!iws.ws.path.empty()) {
		std::tie(frame_currX, frame_currY) = iws.ws.path.back();  // or use std::pair directly
	}

	// e.g., now I can pass them // see PrintTable
	//SomeFunction(frame_currX, frame_currY);

	//if i remove this & only use a global vector i get a segmentation error
	//std::vector < std::vector <int>> MoveTable (RULE_ROW, std::vector<int>(MOVE_COL,0)); 

	// here x

	
	PrintTable(MoveTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, MOVE_COL, possibleDirs, frame_currX, frame_currY);


	//Del_Fill_model_T(model_T);


	PrintTable_2(model_T, disp, {static_cast<float>(-0.75), static_cast<float>(0)}, 0.05f, 0.2f, 0.1f);





	//PrintModelTable(modelTable, disp, {-0.75, 0}, 0.05f, 0.2f, 0.1f);


	
	//print the truth tABLE HERE FIRST

	std::vector<int> directions = {0, 1};                  // L, R
    std::vector<std::string> statuses = {"va", "inv"};
    std::vector<int> rules = {2, 5};


    //PrintDynamicTruthTable(directions, statuses, rules);



	//int num_bits = 3;
	//auto comboTable = GenerateComboTruthTable(num_bits);

	


	// Then pass comboTable to your existing PrintTable function (which uses fixed colHeaders and row headers)
	//PrintTruthTable(comboTable, disp, {-0.75, 1.0}, 0.05f, 0.2f, 0.1f, 7, 8, 4);


	std::vector <int> APcolumns = APcolumns_main;


	// colours
	//disp.DrawText("Must cross [green circle] ", {-0.75, 0.66}, Colors::darkgreen, 0.02f, 0);
	//disp.DrawText("Cant cross [red square] ", {-0.75, 0.69}, Colors::red, 0.02f, 0);
	//disp.DrawText("For [AP] only \n Violates AP/ Cant_cross [orange] ", {-0.75, 0.72}, Colors::orange, 0.02f, 0);
	//disp.DrawText("Rest [blue-green]", {-0.75, 0.75}, Colors::bluegreen, 0.02f, 0);




	//curr state

	//get all acts
	//MAKE SURE to work with solution paths only --- recheck thissss block
	

	//show all possible acts (before applying IRs)
	//std::cout << "[possible actions: ";
	//for (const auto& action : actions) {
		//std::cout << action << " ";
	//}
	//std::cout << " ]\n";

	//std::vector<int> possibleDirs;
	//for (const auto& action : actions) {
	//	possibleDirs.push_back(static_cast<int>(action));
	//}


	//working with rules
	//apply IRs and log results
	std::unordered_map<WitnessAction, ActionType> filteredLogics;
	std::vector<std::string> resultLog;

	//working here

	for (const auto& action : actions) 
	{
		ActionType overallResult = UNKNOWN;

		for (const auto& [ruleID, ruleFunction] : witnessInferenceRules<puzzleWidth, puzzleHeight>) 
		{
			ActionType result = ruleFunction(w, iws.ws, action);
			if (result == MUST_TAKE) {
				//std::cout << "[must_take] " << action << " -> [rule] " << ruleID << ".\n";
				
				//MoveTable[0][1] += 1;  //replace 9 with result

				// Access the numerical value
				int actionValue = static_cast<int>(action);
				//std::cout<<"rID: "<<ruleID<<" , actVal: "<<actionValue;
				MoveTable[ruleID][actionValue] = "M"; //this is showing M so we can log- curr pos AND rule broken here //ideas//working
				//RuleTable[ruleID][0] += 1; // this is tracking count, not violation or occurence

				// here x2
				PrintTable(MoveTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, MOVE_COL, possibleDirs, frame_currX, frame_currY);
				//PrintRuleTable(RuleTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, RULE_COL);
				PrintTable_2(model_T, disp, {static_cast<float>(-0.75), static_cast<float>(0)}, 0.05f, 0.2f, 0.1f);

				

			} else if (result == CANNOT_TAKE) {
				//std::cout << "[cant_take] " << action << " -> [rule] " << ruleID << ".\n";

				//MoveTable[3][2] += 1;  //replace 9 with result

				int actionValue = static_cast<int>(action);


				//rule 5 (but 4 as we start from 0), AP case
				if(ruleID == 4)
				{
					//find the number in the vector
					auto it = std::find(APcolumns.begin(), APcolumns.end(), actionValue);

					//remove it
					if (it != APcolumns.end()) {
						APcolumns.erase(it);
					}

					//del
					//remaining numbers or the "actValue" we need
					for (int num : APcolumns) {
						//std::cout << num << " ";
						MoveTable[ruleID][num] = "S";  //should_take

					}

					//adding the cant_take parts too

					MoveTable[ruleID][actionValue] = "C"; 

					//del

					//std::cout<< "\t act val : "<< actionValue;
					//just no
					//MoveTable[ruleID][actionValue] = "S"; //should_take or should take
				}
				else
				{
					//normal case
					MoveTable[ruleID][actionValue] = "C"; 
					//RuleTable[ruleID][0] += 1; // this is tracking count, not violation or occurence

				}


					// here x3
				PrintTable(MoveTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, MOVE_COL, possibleDirs, frame_currX, frame_currY);
				//PrintRuleTable(RuleTable, disp, -0.75, 0.05f, 0.2f, 0.1f, 7, RULE_ROW, RULE_COL);
				PrintTable_2(model_T, disp, {static_cast<float>(-0.75), static_cast<float>(0)}, 0.05f, 0.2f, 0.1f);
			}
		}
	}






}








int MyCLHandler(char *argument[], int maxNumArgs)
{
//	if (strcmp(argument[0], "-test") == 0)
//	{
//		BaselineTest();
//		exit(0);
//	}
	return 0;
}

void MyDisplayHandler(unsigned long windowID, tKeyboardModifier mod, char key)
{
	switch (key)
	{
		case 't':
//			ParallelExamine(5);
			GenerateUnique44();
//			ExamineMustCross(numRequiredPieces);
//			w.ClearTetrisConstraints();
//			ExamineTetris(4);
//			ExamineMustCrossAndRegions(numRequiredPieces, numSeparationPieces);
//			ExamineMustCrossAnd3Regions(numRequiredPieces, numSeparationPieces);
//			ExamineTriangles(6);
			//ExamineTriangles(puzzleHeight*puzzleWidth);
//			ExamineRegionsAndStars(0);
			break;
		case 'v':
		{
			std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
			GetAllSolutions(w, allSolutions);
			if (allSolutions.size() > 0)
			{
				iws.ws = allSolutions[0];
				iws.currState = InteractiveWitnessState<puzzleWidth, puzzleHeight>::kWaitingRestart;
			}
		}
			break;
		case 's':
		{
			Graphics::Display d;
			//d.FillRect({-1, -1, 1, 1}, Colors::darkgray);
			w.Draw(d);
			w.Draw(d, iws);
			std::string fname = "/Users/nathanst/Desktop/SVG/witness_";
			int count = 0;
			while (FileExists(fname + std::to_string(count) + ".svg"))
			{
				count++;
			}
			printf("Save to '%s'\n", (fname+std::to_string(count)+".svg").c_str());
			MakeSVG(d, (fname+std::to_string(count)+".svg").c_str(), 400, 400, 0, w.SaveToHashString().c_str());

			{
				int wide, high;
				w.GetDimensionsFromHashString(w.SaveToHashString(), wide, high);
			}
		}
			break;
		//case 'r': recording = !recording; break;
		case '\t':
			if (mod != kShiftDown)
//				SetActivePort(windowID, (GetActivePort(windowID)+1)%GetNumPorts(windowID));
//			else
//			{
//				SetNumPorts(windowID, 1+(GetNumPorts(windowID)%MAXPORTS));
//			}
//			break;

		case 'r':
			vp_active = !vp_active; //toggle viewport // changed from actual case 'r'
			break;

		case '[':
			if (best.size() > 0)
			{
				currBoard = (currBoard+(int)(best.size())-1)%best.size();
				Load(currBoard);
				printf("%d of %lu\n", currBoard+1, best.size());
			}
			break;
		case ']':
			if (best.size() > 0)
			{
				currBoard = (currBoard+1)%best.size();
				Load(currBoard);//, numRequiredPieces, numSeparationPieces);
				printf("%d of %lu\n", currBoard+1, best.size());
			}
			break;
		case '{':
			if (best.size() > 0)
			{
				currBoard = (currBoard+(int)(100*best.size())-100)%best.size();
				Load(currBoard);
				printf("%d of %lu\n", currBoard+1, best.size());
			}
			break;
		case '}':
			if (best.size() > 0)
			{
				currBoard = (currBoard+100)%best.size();
				Load(currBoard);//, numRequiredPieces, numSeparationPieces);
				printf("%d of %lu\n", currBoard+1, best.size());
			}
			break;

		case 'o':
			if (iws.ws.path.size() == 0)
			{
				iws.ws.path.push_back({0, 0});
				iws.ws.path.push_back({0, 1});
				iws.ws.path.push_back({1, 1});
			}
			else {
				iws.Reset();
			}
			break;
		default:
			break;
	}
}

/*
extern "C" {
    void triggerRKey() {
        // call the key 'r' 
        MyDisplayHandler(0, kNoModifier, 'r');
    }
}
*/


bool MyClickHandler(unsigned long, int, int, point3d p, tButtonType , tMouseEventType e)
{
	if (e == kMouseDrag) // ignore movement with mouse button down
		return true;

	if (e == kMouseUp)
	{
		if (whichPuzzle < puzzleNum){

			if (puzzleSet[whichPuzzle][0].Click(p, iws)) // found goal
			{

				
					if (puzzleSet[whichPuzzle][0].GoalTestWithTracking(iws.ws)) // comments // on solved, move to next // we inc cnt, why??
					{
						LogPathCompliance(w, iws.ws);
						for (const auto& loc : iws.ws.path) {
							logFile_pathSol << loc.first << "," << loc.second << "\t"; 
						}
						printf("Solved puzzle- %d!\n", whichPuzzle+1);
						std::cout << "[Solved]";
						logFile_rule << " \n\t[Sol] ";

						whichPuzzle++;
						iws.Reset();
						redrawBackground = true;

					}
						
				else {

					LogPathCompliance(w, iws.ws);
					logFile_pathAll << "\n\nInv\n";
					printf("Invalid solution\n");
					logFile_rule << " \n\t[Inv] ";
					std::cout << "[Inv]";

					iws.Reset();
				}

			}
		}
	}

	//use these to log dir moved
	int currX, currY = 0;
	int prevX, prevY = 0;



	//[OLD mouse, drag, working here]
	//bayesian

	if (e == kMouseMove)
{
	//		printf("Move\n");
	if (whichPuzzle < puzzleNum)
		puzzleSet[whichPuzzle][0].Move(p, iws); // viewport 0
		
    //w.Move(p, iws);
	//std::cout <<"\n\t"<<iws.fracCycle * 0.04; //shows time incessantly 

    if (!iws.ws.path.empty()) {
        auto [currX, currY] = iws.ws.path.back();
    }

    bool shouldLog = false;

    if (!iws.ws.path.empty()) {
        auto [currX, currY] = iws.ws.path.back();

        if (!nodes.empty()) {
            auto [lastX, lastY] = nodes.back();
            if (lastX != currX || lastY != currY) {
                nodes.emplace_back(currX, currY);
                shouldLog = true;
            }
        } else {
            nodes.emplace_back(currX, currY);
            shouldLog = true;
            mustTakeViolationCount = 0;
            cantTakeViolationCount = 0;
        }
    }

    static std::vector<int> mustTakeDirections;
    static std::vector<int> violatedRules;
    static std::vector<int> cantTakeDirections;
    static std::vector<int> violatedRules_forCant;
    static int previousDirection = -1;


//shouldLog start
    if (shouldLog) {

		int count_acts = 0;

        auto [currX, currY] = nodes.back();
        auto [lastX, lastY] = (nodes.size() > 1) ? nodes[nodes.size() - 2] : std::make_pair(-1, -1);

		double time_ish = -9.0;

        int directionMoved = -1;
		std::string dirLabel = "Un";
		std::cout << " \t["<<iws.ws.path.back().first <<", "<<iws.ws.path.back().second << "]";
		logFile_ruleAll << " \t["<<iws.ws.path.back().first <<", "<<iws.ws.path.back().second << "]";
		logFile_pathAll << " \t["<<iws.ws.path.back().first <<", "<<iws.ws.path.back().second << "]";

		//working here for strings/ path/ Jul 06

        if (currX > lastX) directionMoved = 3;
        else if (currX < lastX) directionMoved = 2;
        else if (currY > lastY) directionMoved = 0;
        else if (currY < lastY) directionMoved = 1;

		//this is so redundant, I gotta clear my code
		// anyway so this logs all directions, regardless of if you reach the end/goal or not
		switch(directionMoved)
		{
			case 0: 
				if (directionMoved == 0)
				{
					dirLabel = "Up";
					logFile_three << "\t" <<iws.fracCycle*0.04;
					logFile_three << " [u] "<<iws.fracCycle*0.04 <<"\t";
				}
				break;
			case 1: 
				if (directionMoved == 1)
				{
					dirLabel = "Dn";
					logFile_three << "\t" <<iws.fracCycle*0.04;
					
					logFile_three << " [d] "<<iws.fracCycle*0.04 <<"\t";
				}
				break;

			case 2: 
				if (directionMoved == 2)
				{
					dirLabel = "Lt";
					logFile_three << "\t" <<iws.fracCycle*0.04;
					logFile_three << " [l] "<<iws.fracCycle*0.04 <<"\t";
				}
				break;

			case 3: 
				if (directionMoved == 3)
				{
					dirLabel = "Rt";
					logFile_three << "\t" <<iws.fracCycle*0.04;
					logFile_three << " [r] "<<iws.fracCycle*0.04 <<"\t";
				}
				break;
			
			case 4:
				if(directionMoved == 4)
				{
					dirLabel = "St";

					std::cout <<"\n_\t reset timer";
					iws.fracCycle = 0;

					logFile_three << " [s] "<<iws.fracCycle*0.04 <<"\t";

				}
				break;

			case 5:
				if(directionMoved == 5)
				{
					dirLabel = "En";
					logFile_three << " [e] "<<iws.fracCycle*0.04 <<"\t";
				}
				break;

			default:
				dirLabel = "??"; //start or end
		}

        bool isBacktrack = (directionMoved != -1 && previousDirection != -1 &&
                            ((directionMoved == 0 && previousDirection == 1) ||
                             (directionMoved == 1 && previousDirection == 0) ||
                             (directionMoved == 2 && previousDirection == 3) ||
                             (directionMoved == 3 && previousDirection == 2)));

        //std::cout << "\n\ndel Dir: " << directionMoved;
		//std::cout << "\n\ndel Old_Dir: " << previousDirection;

		// when we move to new node/ change dir /and it's not the start
		if (previousDirection != directionMoved && previousDirection != -1 && directionMoved != -1)
		{
			prev_dir = previousDirection; //please make these local
			new_dir = directionMoved;

			//std::cout << "\n\ndel Dir: " << prev_dir; 
			//std::cout << "\n\ndel Old_Dir: " << new_dir;
		}

        logFile << "\n\nMoved: " << directionMoved;
        logFile << "\n\n (" << dirLabel <<") ";
        if (isBacktrack) {
            //std::cout << " (backtrack)";
            //logFile << " (backtrack)";
        }
        //std::cout << "\n\t'You' moved: " << directionMoved << "\n";
        //logFile << "\n";
		//std::cout << "\n\t'You' moved: " << dirLabel << "\n";
		std::cout << " " << dirLabel << "\n";


		logFile_ruleAll << " " << dirLabel << "\n";
        logFile << "\n";
		

        previousDirection = directionMoved;

        StepInfo step;
        step.action = directionMoved;
        step.ruleID = -1;
        step.type = StepType::NORMAL;

        if (!mustTakeDirections.empty() && !violatedRules.empty() && !isBacktrack) {
            if (std::find(mustTakeDirections.begin(), mustTakeDirections.end(), directionMoved) == mustTakeDirections.end()) {
                //std::cout << "violation: must_take\n";
                logFile << "violation: must_take\n";
                //std::cout << "rule: " << GetRuleNameByID(violatedRules.back()) << "\n";
                logFile << "rule: " << GetRuleNameByID(violatedRules.back()) << "\n";
                mustTakeLogger = 1;
                ruleAPLogger = 1;
                RuleTable[violatedRules.back()][1]++;
                step.ruleID = violatedRules.back();
                step.type = StepType::DEVIATE;
                mustTakeDirections.clear();
                violatedRules.clear();
            } else {
                RuleTable[violatedRules.back()][0]++;
                step.ruleID = violatedRules.back();
                step.type = StepType::FOLLOW;
            }
        }

        if (!cantTakeDirections.empty() && !violatedRules_forCant.empty() && !isBacktrack) {
            if (std::find(cantTakeDirections.begin(), cantTakeDirections.end(), directionMoved) != cantTakeDirections.end()) {
                //std::cout << "violation: cant_take\n";
                logFile << "violation: cant_take\n";
                //std::cout << "rule: " << GetRuleNameByID(violatedRules_forCant.back()) << "\n";
                logFile << "rule: " << GetRuleNameByID(violatedRules_forCant.back()) << "\n";
                RuleTable[violatedRules_forCant.back()][1]++;
                step.ruleID = violatedRules_forCant.back();
                step.type = StepType::DEVIATE;
                cantTakeDirections.clear();
                violatedRules_forCant.clear();
            } else {
                RuleTable[violatedRules_forCant.back()][0]++;
                step.ruleID = violatedRules_forCant.back();
                step.type = StepType::FOLLOW;
            }
        }

        std::vector<WitnessAction> actions;
        w.GetActions(iws.ws, actions);



		// issues to fix- backtracking (for this)
		// fix table exclude the previous code
		// just rewrite

		//case where we dont even near the r1 separation block/ node??

//rc is super rule- it can be broke down into a variant of the unfollowed rules in the region???
//must_cross was deviated from {not cant take}
// only gonna get RC, m_c, ATP, R1, towardsGoal {no stars in dataset}
// only two colours
// longer dist m_c is not here- but rc covers that

//how to handle multiple rules??

        //std::cout << "Guess: [";
        logFile << "Guess: [";
        for (size_t i = 0; i < actions.size(); ++i) {

			count_acts = 0;
            //std::cout << static_cast<int>(actions[i]);
            logFile << static_cast<int>(actions[i]);
            if (i < actions.size() - 1) {
				count_acts += 1;
                //std::cout << ", ";// << "\t "<<count_acts;
                logFile << ", ";
            }
        }
        //std::cout << "]\n";
        logFile << "]\n";

        for (const auto& action : actions) {
            ActionType overallResult = UNKNOWN;

            for (const auto& [ruleID, ruleFunction] : witnessInferenceRules<puzzleWidth, puzzleHeight>) {
                ActionType result = ruleFunction(w, iws.ws, action);
                if (result == UNKNOWN) continue;

                if (result == CANNOT_TAKE) {
                    if (mustTakeLogger == 1 && ruleAPLogger == 1) continue;
                    if (directionMoved == action) {
                        cantTakeDirections.push_back(action);
                        violatedRules_forCant.push_back(ruleID);
                        RuleTable[ruleID][2]++;
                    }
                    overallResult = CANNOT_TAKE;
                }

                if (result == MUST_TAKE) {
                    if (std::find(mustTakeDirections.begin(), mustTakeDirections.end(), action) == mustTakeDirections.end()) {
                        mustTakeDirections.push_back(action);
                        violatedRules.push_back(ruleID);
                        RuleTable[ruleID][2]++;
                    }
                    overallResult = MUST_TAKE;
                }

				RuleTable[ruleID][4] = count_acts;
            }
        }

        pathLog.push_back(step);
    }



//shouldLog end





}





	//mouse end, work

	// Don't need any other mouse support
	return true;
}


template <int puzzleWidth, int puzzleHeight>
void DFS(const Witness<puzzleWidth, puzzleHeight> &w,
		 WitnessState<puzzleWidth, puzzleHeight> &s,
		 std::vector<WitnessState<puzzleWidth, puzzleHeight>> &puzzles)
{
	std::vector<WitnessAction> acts;

	if (w.GoalTest(s))
	{
		puzzles.push_back(s);
		return;
	}

	w.GetActions(s, acts);
	for (auto &a : acts)
	{
		w.ApplyAction(s, a);
		DFS(w, s, puzzles);
		w.UndoAction(s, a);
	}
}


template <int puzzleWidth, int puzzleHeight>
void GetAllSolutions(const Witness<puzzleWidth, puzzleHeight> &w, std::vector<WitnessState<puzzleWidth, puzzleHeight>> &puzzles)
{
	WitnessState<puzzleWidth, puzzleHeight> s;
	s.Reset();
	Timer t;
	t.StartTimer();
	DFS(w, s, puzzles);
	t.EndTimer();
	static bool printed = false;
	if (!printed)
		printf("%lu solutions found in %1.2fs\n", puzzles.size(), t.GetElapsedTime());
	printed = true;
}

template <int puzzleWidth, int puzzleHeight>
void GetAllSolutions(std::vector<WitnessState<puzzleWidth, puzzleHeight>> &puzzles)
{
	Witness<puzzleWidth, puzzleHeight> w;
	GetAllSolutions(w, puzzles);
}

void GetAllSolutions()
{
//	std::vector<WitnessState<puzzleWidth, puzzleHeight>> puzzles;
//	GetAllSolutions(puzzles);

	std::vector<WitnessState<2, 2>> p1;
	GetAllSolutions(p1);
	std::vector<WitnessState<2, 3>> p2;
	GetAllSolutions(p2);
	std::vector<WitnessState<3, 3>> p3;
	GetAllSolutions(p3);
	std::vector<WitnessState<3, 4>> p4;
	GetAllSolutions(p4);
	std::vector<WitnessState<4, 4>> p5;
	GetAllSolutions(p5);
	std::vector<WitnessState<4, 5>> p6;
	GetAllSolutions(p6);
	std::vector<WitnessState<5, 5>> p7;
	GetAllSolutions(p7);

}

int CountSolutions(const Witness<puzzleWidth, puzzleHeight> &wp,
				   const std::vector<WitnessState<puzzleWidth, puzzleHeight>> &allSolutions,
				   int &len, int limit)
{
	int count = 0;
	for (const auto &i : allSolutions)
	{
		if (wp.GoalTest(i))
		{
			len = (int)i.path.size();
			count++;
		}
		if (count > limit)
			break;
	}
	return count;
}

int CountSolutions(const Witness<puzzleWidth, puzzleHeight> &w,
				   const std::vector<WitnessState<puzzleWidth, puzzleHeight>> &allSolutions,
				   std::vector<int> &solutions,
				   const std::vector<int> &forbidden,
				   int &len, int limit)
{
	solutions.resize(0);
	int count = 0;
	for (int x = 0; x < forbidden.size(); x++)
	{
		if (w.GoalTest(allSolutions[forbidden[x]]))
			return 0;
	}
	for (int x = 0; x < allSolutions.size(); x++)
	{
		if (w.GoalTest(allSolutions[x]))
		{
			len = (int)allSolutions[x].path.size();
			count++;
			solutions.push_back(x);
		}
		if (count > limit)
			break;
	}
	return count;
}

void Load(uint64_t which)//, int req, int sep)
{
	lock.lock();
	if (best.size() > which)
	{
		w = best[which];
		iws.Reset();
	}
	lock.unlock();
	//	iws.Reset();
//
//		w.ClearMustCrossConstraints();
//		int *items = new int[req];
//		int *items2 = new int[sep];
//		Combinations<w.GetNumMustCrossConstraints()> c;
//		Combinations<w.GetNumSeparationConstraints()> regionCombs;
//
//	if (best.size() > 0)
//	{
//		c.Unrank(best[which], items, req);
//		for (int x = 0; x < req; x++)
//		{
//			w.SetMustCrossConstraint(items[x]);
//		}
//	}
//
//	if (otherbest.size() > 0)
//	{
//		w.ClearSeparationConstraints();
//		uint64_t bits = otherbest[which]; // will only use bottom bits
//		uint64_t hash = otherbest[which]/(1<<sep);
//		regionCombs.Unrank(hash, items2, sep);
//		for (int x = 0; x < sep; x++)
//		{
//			w.AddSeparationConstraint(items2[x], ((bits>>x)&1)?Colors::white:Colors::black);
//		}
//	}
//
//	delete [] items;
//	delete [] items2;
}

void ExamineMustCross(int count)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	uint64_t minCount = allSolutions.size();
	int *items = new int[count];
	Combinations<w.GetNumPathConstraints()> c;


	Witness<puzzleWidth, puzzleHeight> w;
	WitnessState<puzzleWidth, puzzleHeight> s;

	uint64_t maxRank = c.MaxRank(count);
	for (uint64_t n = 0; n < maxRank; n++)
	{
		if (0 == n%50000)
			printf("%llu of %llu\n", n, maxRank);
		c.Unrank(n, items, count);
		for (int x = 0; x < count; x++)
		{
			w.SetMustCrossConstraint(items[x]);
		}

		int pathLen = 0;
		int result = CountSolutions(w, allSolutions, pathLen, minCount+1);
		if (result > minSolutions)
		{
			// ignore
		}
		else if (result < minCount && result > 0)
		{
			minCount = result;
			best.clear();
			best.push_back(w);
		}
		else if (result == minCount)
		{
			best.push_back(w);
		}

		for (int x = 0; x < count; x++)
		{
			w.ClearMustCrossConstraint(items[x]);
		}
	}

	printf("\n%lu boards with %llu solutions; %1.2fs elapsed\n", best.size(), minCount, t.EndTimer());
	if (best.size() > 0)
	{
		currBoard = 0;
		Load(currBoard);
	}

	delete [] items;
}

void ExamineMustCrossAndRegions(int crossCount, int regionCount)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	uint64_t minCount = allSolutions.size();
	int bestPathSize = 0;
	//	std::vector<uint64_t> best;
	int *crossItems = new int[crossCount];
	int *regionItems = new int[regionCount];
	Combinations<w.GetNumPathConstraints()> c;
	Combinations<w.GetNumSeparationConstraints()> regionCombs;

	{

		Witness<puzzleWidth, puzzleHeight> w;
		WitnessState<puzzleWidth, puzzleHeight> s;

		uint64_t maxRank = c.MaxRank(crossCount);
		for (uint64_t n = 0; n < maxRank; n++)
		{
			if (0 == n%50000 && regionCount == 0)
				printf("%llu of %llu\n", n, maxRank);
			c.Unrank(n, crossItems, crossCount);
			for (int x = 0; x < crossCount; x++)
			{
				w.SetMustCrossConstraint(crossItems[x]);
			}
			uint64_t colorComb = pow(2, regionCount);
			for (uint64_t t = 0; t < regionCombs.MaxRank(regionCount)*colorComb; t++)
			{
				uint64_t globalPuzzle = n*regionCombs.MaxRank(regionCount)*colorComb+t;
				if ((0 == globalPuzzle%50000))
					printf("-->%llu of %llu\n", globalPuzzle, maxRank*regionCombs.MaxRank(regionCount)*colorComb);
				uint64_t bits = t; // will only use bottom bits
				uint64_t hash = t/colorComb;

				// easy way to reduce symmetry
				if (t&1) continue;

//				if (0 != bits%3)
//					continue;
//				if (1 != (bits/3)%3)
//					continue;
//				if (2 != (bits/9)%3)
//					continue;

				w.ClearSeparationConstraints();
				regionCombs.Unrank(hash, regionItems, regionCount);
				for (int x = 0; x < regionCount; x++)
				{
					int colour = bits%2;
					bits = bits/2;
//					w.AddSeparationConstraint(regionItems[x], ((bits>>x)&1)?Colors::white:Colors::black);
					w.AddSeparationConstraint(regionItems[x], (colour==0)?Colors::white:((colour==1)?Colors::black:Colors::blue));
				}

				int pathSize = 0;
				int result = CountSolutions(w, allSolutions, pathSize, minCount+1);

				if (result < minCount && result > 0)
				{
					minCount = result;
					bestPathSize = pathSize;
					best.clear();
					best.push_back(w);
				}
				else if (result == minCount && pathSize == bestPathSize)
//				else if (result == minCount)
				{
					best.push_back(w);
				}
			}

			w.ClearPathConstraints();
		}
	}

	printf("\n%lu boards with %llu solutions len %d; %1.2fs elapsed\n", best.size(), minCount, bestPathSize, t.EndTimer());
	if (best.size() > 0)
	{
		currBoard = 0;
		Load(currBoard);
	}

	delete [] crossItems;
	delete [] regionItems;
}

void ExamineMustCrossAnd3Regions(int crossCount, int regionCount)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	uint64_t minCount = allSolutions.size();
	int bestPathSize = 0;
	//	std::vector<uint64_t> best;
	int *crossItems = new int[crossCount];
	int *regionItems = new int[regionCount];
	Combinations<w.GetNumPathConstraints()> c;
//	Combinations<w.GetNumMustCrossConstraints()> c;
	Combinations<w.GetNumSeparationConstraints()> regionCombs;

	{

		Witness<puzzleWidth, puzzleHeight> w;
		WitnessState<puzzleWidth, puzzleHeight> s;

		uint64_t maxRank = c.MaxRank(crossCount);
		for (uint64_t n = 0; n < maxRank; n++)
		{
			if (0 == n%50000 && regionCount == 0)
				printf("%llu of %llu\n", n, maxRank);
			c.Unrank(n, crossItems, crossCount);
			for (int x = 0; x < crossCount; x++)
			{
				w.SetMustCrossConstraint(crossItems[x]);
			}
			uint64_t colorComb = pow(3, regionCount);
			for (uint64_t t = 0; t < regionCombs.MaxRank(regionCount)*colorComb; t++)
			{
				uint64_t globalPuzzle = n*regionCombs.MaxRank(regionCount)*colorComb+t;
				if ((0 == globalPuzzle%50000))
					printf("-->%llu of %llu\n", globalPuzzle, maxRank*regionCombs.MaxRank(regionCount)*colorComb);
				uint64_t bits = t; // will only use bottom bits
				uint64_t hash = t/colorComb;

//				// easy way to reduce symmetry
//				if (t&1) continue;
//
				if (0 != bits%3)
					continue;
				if (1 != (bits/3)%3)
					continue;
				if (2 != (bits/9)%3)
					continue;

				w.ClearSeparationConstraints();
				regionCombs.Unrank(hash, regionItems, regionCount);
				for (int x = 0; x < regionCount; x++)
				{
					int colour = bits%3;
					bits = bits/3;
					w.AddSeparationConstraint(regionItems[x], (colour==0)?Colors::white:((colour==1)?Colors::black:Colors::blue));
				}

				int pathSize = 0;
				int result = CountSolutions(w, allSolutions, pathSize, minCount+1);

				if (result < minCount && result > 0)
				{
					minCount = result;
					bestPathSize = pathSize;
					best.clear();
					best.push_back(w);
				}
				else if (result == minCount && pathSize == bestPathSize)
					//				else if (result == minCount)
				{
					best.push_back(w);
				}
			}

			w.ClearPathConstraints();
		}
	}

	printf("\n%lu boards with %llu solutions len %d; %1.2fs elapsed\n", best.size(), minCount, bestPathSize, t.EndTimer());
	if (best.size() > 0)
	{
		currBoard = 0;
		Load(currBoard);
	}

	delete [] crossItems;
	delete [] regionItems;
}

void ExamineRegionsAndStars(int count)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	uint64_t minCount = allSolutions.size();
	int bestPathSize = 0;
	//	std::vector<uint64_t> best;
	std::vector<int> items(count);
//	Combinations<w.GetNumStarConstraints()> c;
	Combinations<w.GetNumPathConstraints()> mc;
	std::vector<int> forbidden;
	std::vector<int> currSolutions;

	{
		Witness<puzzleWidth, puzzleHeight> w;
		WitnessState<puzzleWidth, puzzleHeight> s;

		if (puzzleWidth!=4 || puzzleHeight!= 4)
		{
			printf("This code only works for 4x4");
			return;
		}
		//static_assert((4==puzzleHeight)&&(puzzleWidth==4), "This code only works for 4x4");
//		uint64_t variations = pow(4, count-1)*2;
//		uint64_t maxRank = c.MaxRank(count)*variations;
		uint64_t variations = pow(4, 8);
		uint64_t mcRank = mc.MaxRank(count);
		uint64_t maxRank = mcRank*variations;//c.MaxRank(count)*variations;

		for (uint64_t n = 0; n < maxRank; n++)
		{
			if (0 == n%50000)
				printf("%llu of %llu\n", n, maxRank);
			w.ClearInnerConstraints();
			w.ClearPathConstraints();
//			uint64_t rankPart = n/variations;
//			uint64_t varPart = n%variations;
//			c.Unrank(rankPart, &items[0], count);
//			for (int i : items)
//			{
//				bool colorPart = (varPart/2)%2;
//				bool shapePart = varPart%2;
//				varPart /= 4;
//				if (shapePart)
//					w.AddStarConstraint(i, colorPart?Colors::black:Colors::blue);
//				else
//					w.AddSeparationConstraint(i, colorPart?Colors::black:Colors::blue);
//			}

			uint64_t rank = n%variations;
			for (int x = 0; x < 4; x++)
			{
				rgbColor color;
				color = (rank&1)?Colors::white:Colors::orange;
				if (rank&2)
					w.AddStarConstraint(x, 1, color);
				else {
					w.AddTriangleConstraint(x, 1, 1+(rank&1));
					//w.AddSeparationConstraint(x, 0, color);
				}
				rank>>=2;

				color = (rank&1)?Colors::white:Colors::orange;
				if (rank&2)
					w.AddStarConstraint(x, 2, color);
				else {
//					w.AddSeparationConstraint(x, 3, color);
					w.AddTriangleConstraint(x, 2, 1+(rank&1));
				}
				rank>>=2;
			}
//			for (int y = 0; y < 4; y+=3)
//			{
//				rgbColor color;
//				color = (rank&2)?Colors::white:Colors::red;
//
//				if (rank&1)
//					w.AddStarConstraint(0, y, color);
//				else
//					w.AddSeparationConstraint(0, y, color);
//				rank>>=2;
//				if (rank&1)
//					w.AddStarConstraint(3, y, color);
//				else
//					w.AddSeparationConstraint(3, y, color);
//				rank>>=2;
//			}
			mc.Unrank(n/variations, &items[0], count);
			for (int x = 0; x < items.size(); x++)
			{
				w.SetMustCrossConstraint(items[x]);
			}

			int pathSize = 0;
			//int result = CountSolutions(w, allSolutions, pathSize, minCount+1);
			int result = CountSolutions(w, allSolutions, currSolutions, forbidden, pathSize, minCount+1);

			// don't return two puzzles with the same solution
			if (currSolutions.size() == 1)
				forbidden.push_back(currSolutions[0]);

			if (result < minCount && result > 0)
			{
				minCount = result;
				bestPathSize = pathSize;
				best.clear();
				best.push_back(w);
			}
			else if (result == minCount && pathSize == bestPathSize)
				//				else if (result == minCount)
			{
				best.push_back(w);
			}
		}
	}

	printf("\n%lu boards with %llu solutions len %d; %1.2fs elapsed\n", best.size(), minCount, bestPathSize, t.EndTimer());
	if (best.size() > 0)
	{
		currBoard = 0;
		Load(currBoard);
	}

}
FILE *f = 0;
int totalCountHere = 0;
void GenerateUnique44Helper(int count, int threadID, int numThreads)
{
	if (f == 0)
		return;
	best.clear();
	currBoard = 0;
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	Witness<puzzleWidth, puzzleHeight> wp;
	WitnessState<puzzleWidth, puzzleHeight> s;

	uint64_t total = 0;
//	for (int count = 0; count <= 16; count++)
	{
		uint64_t minCount = allSolutions.size();
		int bestPathSize = 0;
		std::vector<int> items(count);
		Combinations<wp.GetNumStarConstraints()> c;
		std::vector<int> forbidden;
		std::vector<int> currSolutions;

		//int *items = new int[count];
		
		const int pieceTypes = 2;//24+2;
		
		uint64_t pCount = pow(pieceTypes, count);
		uint64_t maxRank = c.MaxRank(count)*pCount;
		for (uint64_t rank = threadID; rank < maxRank; rank+=numThreads)
		{
			wp.ClearInnerConstraints();
			if (0 == rank%50000)
				printf("%llu of %llu\n", rank, maxRank);
			uint64_t n = rank/pCount; // arrangement on board
			uint64_t pieces = rank%pCount; // pieces in locations
			c.Unrank(n, &items[0], count);
			for (int x = 0; x < count; x++)
			{
				switch (pieces%pieceTypes)
				{
					case 0:
						wp.AddSeparationConstraint(items[x], Colors::black);
						break;
					case 1:
						wp.AddSeparationConstraint(items[x], Colors::blue);
						break;
				}
				pieces/=pieceTypes;
			}
			
			int pathSize = 0;
			
			// find single solution puzzles
			int result = CountSolutions(wp, allSolutions, pathSize, 1);
			
			//lock.lock();
			if (result == 1)
			{
//				Graphics::Display d;
//				d.FillRect({-1, -1, 1, 1}, Colors::darkgray);
//				wp.Draw(d);
//				std::string fname = "/Users/nathanst/Pictures/hog2/witness_"+std::to_string(puzzleWidth)+"x"+std::to_string(puzzleHeight)+"_"+std::to_string(count)+"_";
				lock.lock();
//				while (FileExists(fname + std::to_string(total) + ".svg"))
//				{
//					total++;
//				}
//				printf("Save to '%s'\n", (fname+std::to_string(total)+".svg").c_str());
//				MakeSVG(d, (fname+std::to_string(total)+".svg").c_str(), 400, 400, 0, wp.SaveToHashString().c_str());
				fprintf(f, "%s\n", wp.SaveToHashString().c_str());
				totalCountHere++;
				lock.unlock();
			}
//			best.push_back(wp);
			//lock.unlock();
		}
//		printf("\n%lu boards with %d pieces; %1.2fs elapsed\n", total, count, t.EndTimer());
	}

//	if (best.size() > 0)
//	{
//		currBoard = 0;
//		Load(currBoard);
//	}
//	parallel = false;
}

void GenerateUnique44()
{
	Timer timer;
	const int numThreads = std::thread::hardware_concurrency();
	std::vector<std::thread *> t(numThreads);
	f = fopen("/Users/nathanst/Pictures/hog2/witness_4x4.txt", "a+");
	if (f == 0)
	{
		printf("Failure to open file\n");
		exit(0);
	}
	for (int count = 1; count <= puzzleWidth*puzzleHeight; count++)
	{
		printf("Starting %d\n", count);
		timer.StartTimer();
		for (int x = 0; x < numThreads; x++)
		{
			delete t[x];
			t[x] = new std::thread(GenerateUnique44Helper, count, x, numThreads);
		}
		for (int x = 0; x < numThreads; x++)
		{
			t[x]->join();
			delete t[x];
			t[x] = 0;
		}
		printf("Total: %d\n", totalCountHere);
		printf("%1.2fs elapsed\n", timer.EndTimer());
	}
	printf("Total: %d\n", totalCountHere);
	fclose(f);
	exit(0);
}


void ParallelExamineHelper(int count, int threadID, int numThreads)
{
	best.clear();
	currBoard = 0;
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
	GetAllSolutions(allSolutions);

	Witness<puzzleWidth, puzzleHeight> wp;
	WitnessState<puzzleWidth, puzzleHeight> s;
	//wp.SetGoal(puzzleWidth+1, puzzleHeight);

	uint64_t minCount = allSolutions.size();
	int bestPathSize = 0;
	std::vector<int> items(count);
	Combinations<wp.GetNumStarConstraints()> c;
	//	Combinations<wp.GetNumMustCrossConstraints()> mc;
	std::vector<int> forbidden;
	std::vector<int> currSolutions;

	//int *items = new int[count];

	const int pieceTypes = 3;//24+2;

	uint64_t pCount = pow(pieceTypes, count);
	uint64_t maxRank = c.MaxRank(count)*pCount;
	for (uint64_t rank = threadID; rank < maxRank; rank+=numThreads)
	{
//		wp.ClearTetrisConstraints();
//		wp.ClearStarConstraints();
		wp.ClearInnerConstraints();
		if (0 == rank%50000)
			printf("%llu of %llu\n", rank, maxRank);
		uint64_t n = rank/pCount; // arrangement on board
		uint64_t pieces = rank%pCount; // pieces in locations
		c.Unrank(n, &items[0], count);
//		bool t1 = false, t2 = false, t3 = false, t4 = false, t5 = false, t6 = false;
		for (int x = 0; x < count; x++)
		{
			switch (pieces%pieceTypes)
			{
				case 0:
					wp.AddSeparationConstraint(items[x], Colors::orange);
					break;
				case 1:
					wp.AddStarConstraint(items[x], Colors::orange);
					break;
				case 2:
					wp.AddTriangleConstraint(items[x], 2);
//					wp.AddSeparationConstraint(items[x], Colors::blue);
					break;
				case 3:
					wp.AddStarConstraint(items[x], Colors::blue);
					break;
				case 4:
					wp.AddSeparationConstraint(items[x], Colors::green);//
					break;
				case 5:
					wp.AddStarConstraint(items[x], Colors::green);
					break;
			}
			pieces/=pieceTypes;
		}
//		if (0)
//		{
//			int x=0;
//			if ((pieces%pieceTypes) < 24)
//			{
//				wp.AddTetrisConstraint(items[x], 1+(pieces%pieceTypes));
//				t1 = true;
//			}
//			else {
////				wp.AddStarConstraint(items[x], wp.tetrisYellow);
////				wp.AddTriangleConstraint(items[x], (pieces%pieceTypes)-23);
//				if ((pieces%pieceTypes)-24)
//				{
//					wp.AddSeparationConstraint(items[x], Colors::black);
//					t2 = true;
//				}
//				else {
//					wp.AddSeparationConstraint(items[x], Colors::white);
//					t3 = true;
//				}
//			}
//			pieces/=pieceTypes;
//		}
//		if (!(t1 && t2 && t3))// && t4 && t5 && t6))
//			continue;


		int pathSize = 0;
		int result = CountSolutions(wp, allSolutions, pathSize, minCount+1);
		//int result = CountSolutions(wp, allSolutions, currSolutions, forbidden, pathSize, minCount+1);

			// don't return two puzzles with the same solution
//			if (currSolutions.size() == 1)
//				forbidden.push_back(currSolutions[0]);

		if ((result < minCount && result > 0) || (result == minCount && pathSize > bestPathSize))
		{
			lock.lock();
			printf("Decreased number of solutions to %d / best path up to %d\n", result, bestPathSize);
			minCount = result;
			bestPathSize = pathSize;
			currBoard = 0;
			best.clear();
			best.push_back(wp);
			lock.unlock();
		}
		else if (result == minCount && pathSize == bestPathSize)
			//				else if (result == minCount)
		{
			lock.lock();
			best.push_back(wp);
			lock.unlock();
		}
	}

	printf("\n%lu boards with %llu solutions len %d; %1.2fs elapsed\n", best.size(), minCount, bestPathSize, t.EndTimer());
//	if (best.size() > 0)
//	{
//		currBoard = 0;
//		Load(currBoard);
//	}
	parallel = false;
}

const int numThreads = std::thread::hardware_concurrency();
void ParallelExamine(int count)
{
	std::vector<std::thread *> t(numThreads);
	if (!parallel)
	{
		parallel = true;
		for (int x = 0; x < numThreads; x++)
		{
			delete t[x];
			t[x] = new std::thread(ParallelExamineHelper, count, x, numThreads);
		}
	}
}


//void ExamineRegions(int regionCount)
//{
//	Timer t;
//	t.StartTimer();
//	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;
//	GetAllSolutions(allSolutions);
//
//	uint64_t minCount = allSolutions.size();
//	int bestPathSize = 0;
//	//	std::vector<uint64_t> best;
//	int *regionItems = new int[regionCount];
//	Combinations<w.GetNumMustCrossConstraints()> c;
//	Combinations<w.GetNumSeparationConstraints()> regionCombs;
//
//	{
//
//		Witness<puzzleWidth, puzzleHeight> w;
//		WitnessState<puzzleWidth, puzzleHeight> s;
//
//		for (uint64_t t = 0; t < regionCombs.MaxRank(regionCount)*(1<<regionCount); t++)
//		{
//			if (0 == t%1000)
//				printf("-->%llu of %llu\n", t, regionCombs.MaxRank(regionCount)*(1<<regionCount));
//			uint64_t bits = t; // will only use bottom bits
//			uint64_t hash = t/(1<<regionCount);
//
//			// easy way to reduce symmetry
//			if (t&1) continue;
//
//			w.ClearSeparationConstraints();
//			regionCombs.Unrank(hash, regionItems, regionCount);
//			for (int x = 0; x < regionCount; x++)
//			{
//				w.AddSeparationConstraint(regionItems[x], ((bits>>x)&1)?Colors::white:Colors::black);
//			}
//
//			int pathSize = 0;
//			int result = CountSolutions(w, allSolutions, pathSize, minCount+1);
//			if (result > minSolutions)
//			{
//				// ignore
//			}
//			else if (result < minCount && result > 0)
//			{
//				minCount = result;
//				bestPathSize = pathSize;
//				best.clear();
//				otherbest.clear();
//				best.push_back(n);
//				otherbest.push_back(t);
//			}
//			else if (result == minCount && pathSize > bestPathSize)
//			{
//				minCount = result;
//				bestPathSize = pathSize;
//				best.clear();
//				otherbest.clear();
//				best.push_back(n);
//				otherbest.push_back(t);
//			}
//			else if (result == minCount && pathSize == bestPathSize)
//			{
//				best.push_back(n);
//				otherbest.push_back(t);
//			}
//		}
//
//		w.ClearMustCrossConstraints();
//	}
//
//	printf("\n%lu boards with %llu solutions len %d; %1.2fs elapsed\n", best.size(), minCount, bestPathSize, t.EndTimer());
//	if (best.size() > 0)
//	{
//		currBoard = 0;
//		Load(currBoard, 0, regionCount);
//	}
//
//	delete [] regionItems;
//}


void ExamineTetris(int count)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;

	Witness<puzzleWidth, puzzleHeight> wp;
	WitnessState<puzzleWidth, puzzleHeight> s;

//	for (int y = 0; y < puzzleHeight+1; y++)
//		for (int x = 0; x < puzzleWidth+1; x++)
//			wp.AddMustCrossConstraint(x, y);

	GetAllSolutions(wp, allSolutions);

	uint64_t minCount = allSolutions.size();
	int *items = new int[count];
	Combinations<wp.GetNumTetrisConstraints()> c;

	const int pieceTypes = 24+2;

	uint64_t pCount = pow(pieceTypes, count);
	uint64_t maxRank = c.MaxRank(count)*pCount;
	for (uint64_t rank = 0; rank < maxRank; rank++)
	{
		wp.ClearTetrisConstraints();
		// wp.ClearTriangleConstraints();
		wp.ClearStarConstraints();
		wp.ClearSeparationConstraints();
		if (0 == rank%50000)
			printf("%llu of %llu\n", rank, maxRank);
		uint64_t n = rank/pCount; // arrangement on board
		uint64_t pieces = rank%pCount; // pieces in locations
		c.Unrank(n, items, count);
		bool t1 = false, t2 = false;
		for (int x = 0; x < count; x++)
		{
//			if (x == 0 && count > 1)
//			{
//				wp.AddNegativeTetrisConstraint(items[x], (1+(pieces%24)));
//			}
//			else {
			if ((pieces%pieceTypes) < 24)
			{
				wp.AddTetrisConstraint(items[x], 1+(pieces%pieceTypes));
				t1 = true;
			}
			else {
				//wp.AddStarConstraint(items[x], wp.tetrisYellow);
				//wp.AddTriangleConstraint(items[x], (pieces%pieceTypes)-23);
				if ((pieces%pieceTypes)-23)
					wp.AddSeparationConstraint(items[x], Colors::black);
				else
					wp.AddSeparationConstraint(items[x], Colors::white);
				//wp.AddTriangleConstraint(items[x], (pieces%pieceTypes)-23);
				t2 = true;
			}
//			}
			pieces/=pieceTypes;
		}
		if (!(t1 && t2))
			continue;
//		w = wp;
//		if (rank == 2)
//			break;
		int pathLen = 0;
		int result = CountSolutions(wp, allSolutions, pathLen, minCount+1);
		if (result > minSolutions)
		{
			// ignore
		}
		else if (result < minCount && result > 0)
		{
			minCount = result;
			best.clear();
			best.push_back(wp);
			w = wp;
		}
		else if (result == minCount)
		{
			best.push_back(wp);
		}

	}

	printf("\n%lu boards with %llu solutions; %1.2fs elapsed\n", best.size(), minCount, t.EndTimer());
//	return;

//	if (best.size() > 0)
//	{
//		currBoard = 0;
//		Load(currBoard, count, 0);
//	}

	delete [] items;
}


void ExamineTriangles(int count)
{
	Timer t;
	t.StartTimer();
	std::vector<WitnessState<puzzleWidth, puzzleHeight>> allSolutions;

	Witness<puzzleWidth, puzzleHeight> wp;
	WitnessState<puzzleWidth, puzzleHeight> s;

//	for (int y = 0; y < puzzleHeight+1; y++)
//		for (int x = 0; x < puzzleWidth+1; x++)
//			wp.AddMustCrossConstraint(x, y);

	GetAllSolutions(wp, allSolutions);

	uint64_t minCount = allSolutions.size();
	int *items = new int[count];
	Combinations<wp.GetNumTriangleConstraints()> c;

	const int NUM_TRI = 4;
	uint64_t pCount = pow(NUM_TRI, count);
	uint64_t maxRank = c.MaxRank(count)*pCount;
	for (uint64_t rank = maxRank/10; rank < maxRank && best.size() < 5000; rank+=1)
	{
		if (0 == rank%50000)
			printf("%llu of %llu [%d solutions]\n", rank, maxRank, best.size());
		uint64_t n = rank/pCount; // arrangement on board
		uint64_t pieces = rank%pCount; // pieces in locations
		c.Unrank(n, items, count);
		for (int x = 0; x < count; x++)
		{
			if (0 == pieces%NUM_TRI)
			{
				wp.AddStarConstraint(items[x], wp.triangleColor);
			}
			else {
				wp.AddTriangleConstraint(items[x], 0+(pieces%NUM_TRI));
			}
			pieces/=NUM_TRI;
		}
		int pathLen = 0;
		int result = CountSolutions(wp, allSolutions, pathLen, minCount+1);
		if (result > minSolutions)
		{
			// ignore
		}
		else if (result < minCount && result > 0)
		{
			minCount = result;
			best.clear();
			best.push_back(wp);
			w = wp;
		}
		else if (result == minCount)
		{
			best.push_back(wp);
		}
		wp.ClearStarConstraints();
		wp.ClearTriangleConstraints();
	}

	printf("\n%lu boards with %llu solutions; %1.2fs elapsed\n", best.size(), minCount, t.EndTimer());
	//	return;

	//	if (best.size() > 0)
	//	{
	//		currBoard = 0;
	//		Load(currBoard, count, 0);
	//	}

	delete [] items;
}
