#pragma once

namespace bl::csharp::tile_sprite {

enum class BatchAction { Reuse, Begin, Reject };

inline constexpr BatchAction chooseBatchAction(bool ready,bool batchValid,bool alreadyOpen) {
    if(!ready||!batchValid)return BatchAction::Reject;
    return alreadyOpen?BatchAction::Reuse:BatchAction::Begin;
}

inline constexpr bool shouldEndBatch(BatchAction action,bool beganSuccessfully) {
    return action==BatchAction::Begin&&beganSuccessfully;
}

}
