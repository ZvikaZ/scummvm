/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "common/hashmap.h"
#include "sci/engine/vm_hooks.h"
#include "sci/engine/vm.h"
#include "sci/engine/state.h"
#include "sci/engine/kernel.h"
#include "sci/engine/scriptdebug.h"

namespace Sci {

/** Write here all games hooks
 *  From this we'll build _hooksMap, which contains only relevant hooks to current game
 *  The match is performed according to PC, script number, objName, opcode (only opcode name, as seen in ScummVM debugger),
 *  and either:
 *  - selector	(and then externID is -1)
 *  - external function ID (and then selector is "")
 */
static const GeneralHookEntry allGamesHooks[] = {
	// GID	 ,  PC.seg, PC.offset, script, objName, selector, externID, opcode, function
	{GID_QFG1, {0x0018, 0x144d}, {58, "egoRuns", "changeState", -1 , "push0", &qfg1_die_after_running_on_ice}},
	// the following is example, can be removed when we'll have a concrete usage
	{GID_QFG1, {0x0001, 0x199e}, {0 , "egoRuns", ""           , 36 , "ret",   &qfg1_extern_example}}
};


VmHooks::VmHooks() {
	// build _hooksMap
	for (uint i = 0; i < sizeof(allGamesHooks) / sizeof(GeneralHookEntry); i++) {
		if (allGamesHooks[i].gameId == g_sci->getGameId())		
			_hooksMap.setVal(allGamesHooks[i].key, allGamesHooks[i].entry);
	}
}

uint64 HookHashKey::hash() {
	return ((uint64)segment << 32) + offset;
}


// solves the issue described at #9646:
// "
// When in room 58, and type "run", the hero will fall and his HP will decrease by 1 point. This can be repeated, but will never cause the hero to die.
// When typing "run" the ego will be assigned with the script egoRuns.
// egoRuns::changeState calls proc0_36 in script 0 which is deducing damage from the hero's HP.
// This procedure returns TRUE if the hero is still alive, but the return value is never observed in egoRuns.
// "
// we solve that by calling the hook before executing the opcode following proc0_36 call
// and check the return value. if the hero should die, we kill him
void qfg1_die_after_running_on_ice(Sci::EngineState *s) {
	uint32 return_value = s->r_acc.getOffset();		// I'm assuming the segment is irrelevant

	if (return_value == 0) {
		// 0 mean that the hero should die
		// done according to the code at main.sc, proc0_29:
		// 			(proc0_1 0 59 80 {Death from Overwork} 82 800 1 4)
		PUSH(8);	// params count
		PUSH(0);	// script 0
		PUSH(59);	// extern 59

		// the following commands should be:
		// pushi    80
		// lofsa{ Death from Overwork }
		// push
		//
		// I didn't bother with finding a string address, and the following 2 commands just ignore the string:
		PUSH(0);
		PUSH32(s->xs->addr.pc);

		PUSH(82);
		PUSH(800);
		PUSH(1);
		PUSH(4);

		// CALLE implementation, based on 'case op_calle' above, assuming s->r_rest is zero (or, at least, irrelevant)
		int framesize = 16;
		int temp = ((framesize >> 1) + 0 + 1);
		StackPtr s_temp = s->xs->sp;
		s->xs->sp -= temp;

		ExecStack *xs_new = execute_method(s, 0, 1, s_temp, s->xs->objp, s->xs->sp[0].getOffset(), s->xs->sp);
		if (xs_new) { // in case of error, keep old stack
			s->_executionStackPosChanged = true;
			s->xs = xs_new;
		};
	};
}

// just an example of modifying an extern function
void qfg1_extern_example(Sci::EngineState *s) {
	if (s->r_acc.getOffset() == 0) {
		debugC(kDebugLevelPatcher, "0_36 has decided that you're going to die");
	}
}

// returns true if entry is matching to current state
bool hook_exec_match(Sci::EngineState *s, HookEntry entry) {
	Script *scr = s->_segMan->getScript(s->xs->addr.pc.getSegment());
	int scriptNumber = scr->getScriptNumber();
	const char *objName = s->_segMan->getObjectName(s->xs->objp);
	Common::String selector = nullptr;
	if (s->xs->debugSelector != -1)
		selector = g_sci->getKernel()->getSelectorName(s->xs->debugSelector);
	byte opcode = (scr->getBuf(s->xs->addr.pc.getOffset())[0]) >> 1;

	return scriptNumber == entry.scriptNumber && strcmp(objName, entry.objName) == 0 && selector == entry.selector &&
		s->xs->debugExportId == entry.exportId && strcmp(entry.opcodeName, opcodeNames[opcode]) == 0;
}


void VmHooks::vm_hook_before_exec(Sci::EngineState *s) {
	HookHashKey key = { s->xs->addr.pc.getSegment(), s->xs->addr.pc.getOffset() };
	if (_hooksMap.contains(key)) {
		HookEntry entry = _hooksMap[key];
		if (hook_exec_match(s, entry)) {
			debugC(kDebugLevelPatcher, "vm_hook: patching script: %d, PC: %04x:%04x, obj: %s, selector: %s, extern: %d, opcode: %s", entry.scriptNumber, PRINT_REG(s->xs->addr.pc), entry.objName, entry.selector.c_str(), entry.exportId, entry.opcodeName);
			entry.func(s);
		} else {
			debugC(kDebugLevelPatcher, "vm_hook: failed to match! script: %d, PC: %04x:%04x, obj: %s, selector: %s, extern: %d, opcode: %s", entry.scriptNumber, PRINT_REG(s->xs->addr.pc), entry.objName, entry.selector.c_str(), entry.exportId, entry.opcodeName);
		}
	}
}

} // End of namespace Sci
