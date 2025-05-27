//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

// Gshare predictor data structures
uint32_t ghr;  // Global History Register
uint8_t *bht;  // Branch History Table (2-bit predictors)

// Tournament predictor data structures
uint8_t *global_bht; // Global predictor table (2-bit counters)
uint32_t *local_history_table; // Local history table (per-PC)
uint8_t *local_bht; // Local predictor table (2-bit counters)
uint8_t *choice_bht; // Choice predictor table (2-bit counters)

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
void
init_predictor()
{

  // Initialize GHR to all zeros
  ghr = 0;
  
  /*
  GSHARE
  */
  if (bpType == GSHARE){
    // Allocate BHT with 2^ghistoryBits entries
    bht = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
    
    // Initialize all BHT entries to WN (Weakly Not Taken)
    for (int i = 0; i < (1 << ghistoryBits); i++) {
      bht[i] = WN;
    }
  }

  /*
  TOURNAMENT
  */
  if (bpType == TOURNAMENT) {
    // Global predictor: 2^ghistoryBits entries
    global_bht = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
    for (int i = 0; i < (1 << ghistoryBits); i++)
      global_bht[i] = WN;

    // Local history table: 2^pcIndexBits entries
    local_history_table = (uint32_t *)malloc((1 << pcIndexBits) * sizeof(uint32_t));
    for (int i = 0; i < (1 << pcIndexBits); i++)
      local_history_table[i] = 0;

    // Local predictor: 2^lhistoryBits entries
    local_bht = (uint8_t *)malloc((1 << lhistoryBits) * sizeof(uint8_t));
    for (int i = 0; i < (1 << lhistoryBits); i++)
      local_bht[i] = WN;

    // Choice predictor: 2^ghistoryBits entries
    choice_bht = (uint8_t *)malloc((1 << ghistoryBits) * sizeof(uint8_t));
    for (int i = 0; i < (1 << ghistoryBits); i++)
      choice_bht[i] = WT; // Weakly prefer global
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE: {
      // Get lower ghistoryBits of PC
      uint32_t pc_lower_bits = pc & ((1 << ghistoryBits) - 1);
      // XOR with GHR to get index
      uint32_t index = pc_lower_bits ^ ghr;
      // Get prediction from BHT
      return (bht[index] >= WT) ? TAKEN : NOTTAKEN;
    }
    case TOURNAMENT: {
      // Global prediction
      uint32_t global_idx = ghr & ((1 << ghistoryBits) - 1);
      uint8_t global_pred = (global_bht[global_idx] >= WT) ? TAKEN : NOTTAKEN;
      // Local prediction
      uint32_t pc_idx = pc & ((1 << pcIndexBits) - 1);
      uint32_t local_history = local_history_table[pc_idx] & ((1 << lhistoryBits) - 1);
      uint8_t local_pred = (local_bht[local_history] >= WT) ? TAKEN : NOTTAKEN;
      // Choice
      uint8_t choice = choice_bht[global_idx];
      if (choice >= WT) {
        return global_pred;
      } else {
        return local_pred;
      }
    }
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case GSHARE: {
      // Get lower ghistoryBits of PC
      uint32_t pc_lower_bits = pc & ((1 << ghistoryBits) - 1);
      // XOR with GHR to get index
      uint32_t index = pc_lower_bits ^ ghr;
      
      // Update BHT based on outcome
      if (outcome == TAKEN) {
        if (bht[index] < ST) bht[index]++;
      } else {
        if (bht[index] > SN) bht[index]--;
      }
      
      // Update GHR by shifting in the outcome
      ghr = (ghr << 1) | outcome;
      // Keep only ghistoryBits
      ghr &= (1 << ghistoryBits) - 1;
      break;
    }
    case TOURNAMENT: {
      // Global
      uint32_t global_idx = ghr & ((1 << ghistoryBits) - 1);
      uint8_t global_pred = (global_bht[global_idx] >= WT) ? TAKEN : NOTTAKEN;
      // Local
      uint32_t pc_idx = pc & ((1 << pcIndexBits) - 1);
      uint32_t local_history = local_history_table[pc_idx] & ((1 << lhistoryBits) - 1);
      uint8_t local_pred = (local_bht[local_history] >= WT) ? TAKEN : NOTTAKEN;
      // Choice
      uint8_t choice = choice_bht[global_idx];
      // Update global predictor
      if (outcome == TAKEN) {
        if (global_bht[global_idx] < ST) global_bht[global_idx]++;
      } else {
        if (global_bht[global_idx] > SN) global_bht[global_idx]--;
      }
      // Update local predictor
      if (outcome == TAKEN) {
        if (local_bht[local_history] < ST) local_bht[local_history]++;
      } else {
        if (local_bht[local_history] > SN) local_bht[local_history]--;
      }
      // Update choice predictor
      if (global_pred != local_pred) {
        if (global_pred == outcome && choice < ST) choice_bht[global_idx]++;
        else if (local_pred == outcome && choice > SN) choice_bht[global_idx]--;
      }
      // Update local history
      local_history_table[pc_idx] = ((local_history << 1) | outcome) & ((1 << lhistoryBits) - 1);
      // Update global history
      ghr = ((ghr << 1) | outcome) & ((1 << ghistoryBits) - 1);
      break;
    }
    default:
      break;
  }
}
