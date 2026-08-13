/*
 * ActionMap.h -- acceso a la tabla de bindings dorada (ver ActionMap.cpp).
 */
#ifndef UI_INPUT_ACTION_MAP_H_
#define UI_INPUT_ACTION_MAP_H_

#include "ChordResolver.h"

namespace UI {
namespace Input {

#define BIND(action_, require_, forbid_, prov_) \
    { (action_), (require_), (forbid_), (prov_) }

}  /* namespace Input */
}  /* namespace UI */

#endif  /* UI_INPUT_ACTION_MAP_H_ */