#ifndef FRAMELAYOUT_H
#define FRAMELAYOUT_H

#include "../config/FrameFieldDef.h"

namespace FrameLayout {

enum class IncompleteLengthPolicy {
    ReturnInvalid,
    UseStaticLayoutSize,
};

struct FrameSizeResult {
    int size = 0;
    bool valid = false;
    bool usedLengthField = false;
    bool lengthFieldIncomplete = false;
};

FrameSizeResult resolveFrameSize(const FrameConfig &config,
                                 const QByteArray &data,
                                 int offset,
                                 IncompleteLengthPolicy incompletePolicy);

}

#endif // FRAMELAYOUT_H
