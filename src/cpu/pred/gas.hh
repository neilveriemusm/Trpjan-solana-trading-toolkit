#ifndef __CPU_PRED_GAS_PRED_HH__
#define __CPU_PRED_GAS_PRED_HH__

#include "cpu/pred/bpred_unit.hh"
#include "cpu/pred/sat_counter.hh"
#include "params/GASBP.hh"

class GASBP : public BPredUnit {

public:
  GASBP(const GASBPParams *params);

  /**
   * Looks up the given address in the branch predictor and returns
   * a true/false value as to whether it is taken.  Also creates a
   * BPHistory object to store any state it will need on squash/update.
   * @param branch_addr The address of the branch to look up.
   * @param bp_history Pointer that will be set to the BPHistory object.
   * @return Whether or not the branch is taken.
   */
  bool lookup(ThreadID tid, Addr branch_addr, void *&bp_history);

  /**
   * Records that there was an unconditional branch, and modifies
   * the bp history to point to an object that has the previous
   * global history stored in it.
   * @param bp_history Pointer that will be set to the BPHistory object.
   */
  void uncondBranch(ThreadID tid, Addr pc, void *&bp_history);
  /**
   * Updates the branch predictor to Not Taken if a BTB entry is
   * invalid or not found.
   * @param branch_addr The address of the branch to look up.
   * @param bp_history Pointer to any bp history state.
   * @return Whether or not the branch is taken.
   */
  void btbUpdate(ThreadID tid, Addr branch_addr, void *&bp_history);
  /**
   * Updates the branch predictor with the actual result of a branch.
   * @param branch_addr The address of the branch to update.
   * @param taken Whether or not the branch was taken.
   * @param bp_history Pointer to the BPHistory object that was created
   * when the branch was predicted.
   * @param squashed is set when this function is called during a squash
   * operation.
   */
  void update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
              bool squashed);

  /**
   * Restores the global branch history on a squash.
   * @param bp_history Pointer to the BPHistory object that has the
   * previous global branch history in it.
   */
  void squash(ThreadID tid, void *bp_history);

  inline unsigned getGHR(ThreadID tid, void *bp_history) const;

private:
  unsigned localCtrBits;
  unsigned bhrBits, phtBits;

  std::vector<unsigned> globalHistoryRegs;
  std::vector<std::vector<SatCounter>> setOfPHTs;

  unsigned globalMask, patternMask;

  inline unsigned getSetIndex(Addr &branch_addr);
  inline unsigned getPHTIndex(ThreadID tid);
  inline bool getPrediction(uint8_t &count);
  inline void updateGlobalHistoryReg(ThreadID tid, bool taken);

  struct BPHistory {
    unsigned globalHistoryReg;
    bool takenPred;
  };
};

#endif // __CPU_PRED_GAS_PRED_HH__
