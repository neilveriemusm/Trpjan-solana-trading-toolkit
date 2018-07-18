#include "cpu/pred/gas.hh"
#include "base/bitfield.hh"
#include "debug/Fetch.hh"

GASBP::GASBP(const GASBPParams *params)
    : BPredUnit(params), localCtrBits(params->localCtrBits),
      bhrBits(params->bhrBits), phtBits(params->phtBits),
      globalHistoryRegs(params->numThreads, 0) {

  unsigned numberOfPHTs, sizeOfPHTSet;

  numberOfPHTs = 1 << phtBits;
  sizeOfPHTSet = 1 << bhrBits;

  setOfPHTs.resize(numberOfPHTs);

  globalMask = mask(bhrBits);
  patternMask = mask(phtBits);

  for (unsigned i = 0; i < numberOfPHTs; ++i) {
    setOfPHTs[i].resize(sizeOfPHTSet);

    for (unsigned j = 0; j < sizeOfPHTSet; ++j)
      setOfPHTs[i][j].setBits(localCtrBits);
  }
}

bool GASBP::lookup(ThreadID tid, Addr branch_addr, void *&bp_history) {
  bool taken;
  unsigned phtIdx, setIdx;
  uint8_t counter;

  setIdx = getSetIndex(branch_addr);
  phtIdx = getPHTIndex(tid);

  DPRINTF(Fetch, "Looking up setIdx %#x and phtIdx %#x\n", setIdx, phtIdx);

  counter = setOfPHTs[setIdx][phtIdx].read();
  taken = getPrediction(counter);

  BPHistory *history = new BPHistory;
  history->globalHistoryReg = globalHistoryRegs[tid];
  history->takenPred = taken;
  bp_history = static_cast<void *>(history);
  updateGlobalHistoryReg(tid, taken);

  return taken;
}

void GASBP::uncondBranch(ThreadID tid, Addr pc, void *&bp_history) {
  BPHistory *history = new BPHistory;
  history->globalHistoryReg = globalHistoryRegs[tid];
  history->takenPred = true;
  bp_history = static_cast<void *>(history);
  updateGlobalHistoryReg(tid, true);
}

inline void GASBP::btbUpdate(ThreadID tid, Addr branch_addr,
                             void *&bp_history) {
  globalHistoryRegs[tid] &= (globalMask & ~ULL(1));
}

void GASBP::update(ThreadID tid, Addr branch_addr, bool taken, void *bp_history,
                   bool squashed) {
  assert(bp_history);

  BPHistory *history = static_cast<BPHistory *>(bp_history);

  // When squashed, just restore the global history register.
  if (squashed) {
    globalHistoryRegs[tid] = (history->globalHistoryReg << 1) | taken;
    return;
  }

  unsigned setIdx, phtIdx;

  setIdx = getSetIndex(branch_addr);
  phtIdx = getPHTIndex(tid);

  DPRINTF(Fetch, "Looking up setIdx %#x and phtIdx %#x\n", setIdx, phtIdx);

  if (taken) {
    DPRINTF(Fetch, "Branch updated as taken.\n");
    setOfPHTs[setIdx][phtIdx].increment();
  } else {
    DPRINTF(Fetch, "Branch updated as not taken.\n");
    setOfPHTs[setIdx][phtIdx].decrement();
  }

  delete history;
}

void GASBP::squash(ThreadID tid, void *bp_history) {
  BPHistory *history = static_cast<BPHistory *>(bp_history);
  globalHistoryRegs[tid] = history->globalHistoryReg;

  delete history;
}

inline unsigned GASBP::getGHR(ThreadID tid, void *bp_history) const {
  return static_cast<BPHistory *>(bp_history)->globalHistoryReg;
}

/// PRIVATE METHODS

inline void GASBP::updateGlobalHistoryReg(ThreadID tid, bool taken) {
  globalHistoryRegs[tid] = (globalHistoryRegs[tid] << 1) | taken;
}

inline unsigned GASBP::getPHTIndex(ThreadID tid) {
  return globalHistoryRegs[tid] & globalMask;
}

inline unsigned GASBP::getSetIndex(Addr &branch_addr) {
  return (branch_addr >> instShiftAmt) & patternMask;
}

inline bool GASBP::getPrediction(uint8_t &count) {
  return (count >> (localCtrBits - 1));
}

GASBP *GASBPParams::create() { return new GASBP(this); }
