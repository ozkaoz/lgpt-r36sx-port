#ifndef _UI_PERCENT_VAR_FIELD_H_
#define _UI_PERCENT_VAR_FIELD_H_

#include "UIIntVarField.h"
#include "Application/FX/FxParamDescriptor.h"

// FXP_DESCRIPTORS_V1 (bacon-1.5, item 1): campo de edicion/display de un
// parametro continuo en la vista comun 0..100 %.  El Variable subyacente
// sigue guardando el valor legacy raw (00-FF) intacto: la serializacion
// de proyectos no cambia.  Draw muestra el percent derivado via el
// descriptor (fxRawToPercent) y ProcessArrow convierte el percent editado
// de vuelta a raw (fxPercentToRaw) con clamp.  Undo (Capture/Restore) y
// A+B (Reset) operan sobre el raw, igual que UIIntVarField.
class UIPercentVarField: public UIIntVarField {

public:

	UIPercentVarField(
    GUIPoint &position,
    Variable &v,
    const FxParamDescriptor &desc,
    const char *label,
    int xOffset,
    int yOffset);

	virtual ~UIPercentVarField() {} ;
	virtual void Draw(GUIWindow &w,int offset=0) ;
	virtual void ProcessArrow(unsigned short mask) ;

protected:
	const FxParamDescriptor &desc_ ;
	const char *label_ ;
} ;

#endif