#include "high_level_search.hpp"
#include <string>
#include <unordered_map>
#include "params.hpp"
using namespace std;

#define UNEXPLORED_PENALTY -500   // A penalty for placements that weren't explored with playouts (could be worse than the eval indicates)
#define MAP_OFFSET 20000          // An offset to make any placement better than the default 0 in the map

/** Concatenates the position of a piece into a single string. */
std::string encodeLockPosition(LockLocation lockLocation){
  char buffer[10];
  sprintf(buffer, "%d|%d|%d", lockLocation.rotationIndex, lockLocation.x, lockLocation.y);
  return string(buffer);
}

/** Calculates the valuation of every possible terminal position for a given piece on a given board, and stores it in a map. */
std::string getLockValueLookupEncoded(GameState gameState, const Piece *firstPiece, const Piece *secondPiece, int keepTopN, const EvalContext *evalContext, const PieceRangeContext pieceRangeContextLookup[3]){
  unordered_map<string, float> lockValueMap;
  unordered_map<string, int> lockValueRepeatMap;
  int numSorted = keepTopN * 2;

  // Get the list of evaluated possibilities
  list<Depth2Possibility> possibilityList;
  searchDepth2(gameState, firstPiece, secondPiece, numSorted, evalContext, possibilityList);

  // Perform playouts on the promising possibilities
  int i = 0;
  int numPlayedOut = 0;
  for (Depth2Possibility const& possibility : possibilityList) {
    string lockPosEncoded = encodeLockPosition(possibility.firstPlacement);
    // Cap the number of times a lock position can be repeated (despite differing second placements)
    int shouldPlayout = i < numSorted && numPlayedOut < keepTopN && lockValueRepeatMap[lockPosEncoded] < 3;
    float overallScore = MAP_OFFSET + (shouldPlayout
      ? possibility.immediateReward + getPlayoutScore(possibility.resultingState, pieceRangeContextLookup, secondPiece->index)
      : possibility.immediateReward + possibility.evalScore + UNEXPLORED_PENALTY);
    if (overallScore > lockValueMap[lockPosEncoded]) {
      if (PLAYOUT_LOGGING_ENABLED) {
        printf("Adding to map: %s %f (%f + %f)\n", lockPosEncoded.c_str(), overallScore - MAP_OFFSET, possibility.immediateReward, overallScore - possibility.immediateReward - MAP_OFFSET);
      }
      lockValueMap[lockPosEncoded] = overallScore;
      lockValueRepeatMap[lockPosEncoded] += 1;
    }
    i++;
    if (shouldPlayout) {
      numPlayedOut++;
    }
  }

  // Encode lookup to JSON
  std::string mapEncoded = std::string("{");
  for( const auto& n : lockValueMap ) {
    char buf[20];
    sprintf(buf, "\"%s\":%f,", n.first.c_str(), n.second - MAP_OFFSET);
    mapEncoded.append(buf);
  }
  if (lockValueMap.size() > 0) {
    mapEncoded.pop_back(); // Remove the last comma
  }
  mapEncoded.append("}");
  return mapEncoded;
}



/** Searches 2-ply from a starting state, and performs a fast eval on each of the resulting states. Maintains a sorted list of the top N possibilities, and adds all the rest onto the end in no specified order. */
int searchDepth2(GameState gameState, const Piece *firstPiece, const Piece *secondPiece, int keepTopN, const EvalContext *evalContext, OUT list<Depth2Possibility> &possibilityList){
  auto cutoffPossibility = possibilityList.begin(); // The node on the "cutoff" between being in the top N placements and not
  int size = 0; // Tracking manually is cheaper than doing the O(n) operation each iteration

  // Get the placements of the first piece
  vector<LockPlacement> firstLockPlacements;
  moveSearch(gameState, firstPiece, evalContext->pieceRangeContext.inputFrameTimeline, firstLockPlacements);
  for (auto it = begin(firstLockPlacements); it != end(firstLockPlacements); ++it) {
    LockPlacement firstPlacement = *it;
    GameState afterFirstMove = advanceGameState(gameState, firstPlacement, evalContext);
    for (int i = 0; i < 19; i++) {
      maybePrint("%d ", afterFirstMove.board[i] & ALL_TUCK_SETUP_BITS);
    }
    maybePrint("%d end of post first move\n", afterFirstMove.board[19] & ALL_TUCK_SETUP_BITS);
    float firstMoveReward = getLineClearFactor(afterFirstMove.lines - gameState.lines, evalContext->weights, evalContext->shouldRewardLineClears);

    // Get the placements of the second piece
    vector<LockPlacement> secondLockPlacements;
    moveSearch(afterFirstMove, secondPiece, evalContext->pieceRangeContext.inputFrameTimeline, secondLockPlacements);

    for (auto secondPlacement : secondLockPlacements) {
      GameState resultingState = advanceGameState(afterFirstMove, secondPlacement, evalContext);
      float evalScore = firstMoveReward + fastEval(afterFirstMove, resultingState, secondPlacement, evalContext);
      float secondMoveReward = getLineClearFactor(resultingState.lines - afterFirstMove.lines, evalContext->weights, evalContext->shouldRewardLineClears);

      Depth2Possibility newPossibility = {
        { firstPlacement.x, firstPlacement.y, firstPlacement.rotationIndex },
        { secondPlacement.x, secondPlacement.y, secondPlacement.rotationIndex },
        resultingState,
        evalScore,
        firstMoveReward + secondMoveReward
      };

      if (size < keepTopN || evalScore > cutoffPossibility->evalScore) {
        // Insert into the list in its correct sorted place
        for (auto it = possibilityList.begin(); true; it++) {
          if (it == possibilityList.end()) {
            possibilityList.push_back(newPossibility);
            size++;
            break;
          }
          if (evalScore > it->evalScore) {
            possibilityList.insert(it, newPossibility);
            size++;
            break;
          }
        }

        // Update the cutoff node
        if (size == keepTopN) {
          // Get the Nth elt
          auto ptr = possibilityList.begin();
          advance(ptr, keepTopN - 1);
          cutoffPossibility = ptr;
        }
        if (size > keepTopN) {
          // Use the predecessor of the one that just got pushed out
          cutoffPossibility = prev(cutoffPossibility);
        }

      } else {
        // Add it at the end of the list (sorting not important)
        possibilityList.push_back(newPossibility);
      }
    }
  }
  return (int) possibilityList.size();
}


// void evaluatePossibilitiesWithPlayouts(int timeoutMs){
//   auto millisec_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
//
// }
