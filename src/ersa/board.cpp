#include "ersa/board/board.h"

namespace ersa {
namespace board {

static Board* s_currentBoard = nullptr;

Board& Board::current() {
    return *s_currentBoard;
}

Board* Board::currentOrNull() {
    return s_currentBoard;
}

void Board::setCurrent(Board* board) {
    s_currentBoard = board;
}

} // namespace board
} // namespace ersa
