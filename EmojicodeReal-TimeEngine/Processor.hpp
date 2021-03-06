//
//  Processor.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 04/01/2017.
//  Copyright © 2017 Theo Weidmann. All rights reserved.
//

#ifndef Processor_hpp
#define Processor_hpp

#include "Engine.hpp"

void performFunction(Function *function, Value self, Thread *thread, Value *destination);
void produce(EmojicodeInstruction coin, Thread *thread, Value *destination);

#endif /* Processor_hpp */
