// PATTERN: decaying passive — hand size that shrinks per round.
#include "../balatro.hpp"

namespace {

class TurtleBean : public Joker {
    int bonus_ = 5;
public:
    void passiveModifiers(PassiveMods& m) const override { m.handSize += bonus_; }
    void interact(Balatro& g, int type, const EventData&, bool isCopy) override {
        if (type != ROUND_END || isCopy) return;
        if (--bonus_ <= 0) g.queueDestroyJoker(*this);
        g.recomputeMods();
    }
};
REGISTER_JOKER(TurtleBean, "Turtle Bean", 2, 6)

}  // namespace
