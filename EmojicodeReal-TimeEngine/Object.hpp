//
//  Object.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 06/02/2017.
//  Copyright © 2017 Theo Weidmann. All rights reserved.
//

#ifndef Object_hpp
#define Object_hpp

#include "Engine.hpp"

#ifndef heapSize
#define heapSize (512 * 1000 * 1000)  // 512 MB
#endif

/// This method is called during the initialization of the Engine.
/// @warning Obviously, you should not call it anywhere else!
void allocateHeap();

template <typename T>
inline void markByObjectVariableRecord(ObjectVariableRecord &record, Value *va, T &index) {
    switch (record.type) {
        case ObjectVariableType::Simple:
            mark(&va[record.variableIndex].object);
            break;
        case ObjectVariableType::Condition:
            if (va[record.condition].raw)
                mark(&va[record.variableIndex].object);
            break;
        case ObjectVariableType::Box:
            if (va[record.variableIndex].raw == T_OBJECT)
                mark(&va[record.variableIndex + 1].object);
            break;
        case ObjectVariableType::ConditionalSkip:
            if (!va[record.condition].raw)
                index += record.variableIndex;
            break;
    }
}

#endif /* Object_hpp */
