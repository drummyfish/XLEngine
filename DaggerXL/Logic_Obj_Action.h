#ifndef LOGIC_OBJ_ACTION_H
#define LOGIC_OBJ_ACTION_H

#include "../plugin_framework/plugin.h"
#include "../world/ObjectDef.h"
#include <string>

class Logic_Obj_Action
{
public:
	Logic_Obj_Action(const XLEngine_Plugin_API *API);
	~Logic_Obj_Action(void);

private:
	const XLEngine_Plugin_API *m_pAPI;

	void LogicSetup(u32 uObjID, u32 uParamCount, LogicParam *param);
	void ObjectSetup(u32 uObjID, u32 uParamCount, LogicParam *param);
	void Update(u32 uObjID, u32 uParamCount, LogicParam *param);
	void Message(u32 uObjID, u32 uParamCount, LogicParam *param);

	LOGIC_CB_FUNC();
};

#endif //LOGIC_OBJ_ACTION_H