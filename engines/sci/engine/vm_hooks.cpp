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

#include "sci/engine/vm_hooks.h"
#include "sci/engine/vm.h"
#include "sci/engine/state.h"
#include "sci/engine/kernel.h"
#include "sci/engine/scriptdebug.h"

namespace Sci {

// TODO:
// - make a table per game, load specific game, verify against table - use common/hashmap
// - make function call from table
// - hook extern example
// - debug prints
// - check the PUSH32 in the middle
// - check the 50/80 in the middle
// - check other similar problems? maybe fix at the health-decrease-export-function?
// - document new code in vm.h, and all vm_hooks code
// - check on Linux
// - check with 'gitk 07df6cc254' if needs more changes
// - fix QFG1VGA


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
		PUSH(50);	// TODO: 50 or 80? maybe 0x50???....

		// that's wrong!!! temp replacement for
		//		lofsa    {Death from Overwork}
		//		push
		// hope that it'll work...
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

bool hook_exec_match(Sci::EngineState *s, int patchScriptNumber, const char *patchObjName, Common::String patchSelector, SegmentId patchSegment, uint32 patchOffset) {
	int scriptNumber = s->_segMan->getScript(s->xs->addr.pc.getSegment())->getScriptNumber();
	const char *objName = s->_segMan->getObjectName(s->xs->objp);
	Common::String selector = nullptr;
	if (s->xs->debugSelector != -1)
		selector = g_sci->getKernel()->getSelectorName(s->xs->debugSelector);

	SegmentId segment = s->xs->addr.pc.getSegment();
	uint32 offset = s->xs->addr.pc.getOffset();

	return scriptNumber == patchScriptNumber && strcmp(objName, patchObjName) == 0 && selector == patchSelector &&
		segment == patchSegment && offset == patchOffset;
}


void vm_hook_before_exec(Sci::EngineState *s) {
	//will be from table:
	const char patchOpcodeName[] = "push0";

	// not working now, TODO fix: void func(Sci::EngineState *s) = qfg1_die_after_running_on_ice;
	//
	// 

	if (hook_exec_match(s, 58, "egoRuns", "changeState", 0x0018, 0x144d)) {
		Script *scr = s->_segMan->getScript(s->xs->addr.pc.getSegment());
		byte opcode = (scr->getBuf(s->xs->addr.pc.getOffset())[0]) >> 1;
		if (strcmp(patchOpcodeName, opcodeNames[opcode])) {
			warning("vm_hook_before_exec: opcode mismatch, ignoring.\n*** Please report to bugs.scummvm.com ***\nscript: %d, object: %s, selector: %s, PC: %4x:%4x, expected opcode: %s, actual opcode: %s", 58, "egoRuns", "changeState", PRINT_REG(s->xs->addr.pc), patchOpcodeName, opcodeNames[opcode]);
		} else {
			qfg1_die_after_running_on_ice(s);
			//TODO debug message patching
			//TODO fix func(s);
		}
	}
}

} // End of namespace Sci
