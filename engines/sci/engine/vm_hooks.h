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

#ifndef SCI_ENGINE_VM_HOOKS_H
#define SCI_ENGINE_VM_HOOKS_H

#include "sci/engine/vm.h"

namespace Sci {


/** Hook functions to call upon match */
void qfg1_die_after_running_on_ice(Sci::EngineState *s);
void qfg1_extern_example(Sci::EngineState *s);

/** Hook function type, to be called upon match */
typedef	void (*HookFunction)(Sci::EngineState *);

/** _hooksMap keys are built from PC's segment and offset */
struct HookHashKey {
	SegmentId segment;
	uint32 offset;

	uint64 hash();

	bool operator==(const HookHashKey &other) const {
		return segment == other.segment && offset == other.offset;
	}


};

/** _hooksMap value entry */
struct HookEntry {
	/** These are used to make sure that the PC is indeed the requested place */
	int scriptNumber;
	const char *objName;
	Common::String selector;
	int exportId;
	const char *opcodeName;

	/** If all the previous match, call func */
	HookFunction func;
};

/** Used for allGamesHooks - from it we build the specific _hooksMap */
struct GeneralHookEntry {
	SciGameId gameId;
	HookHashKey key;
	HookEntry entry;
};

/** Hash key equality function */
struct HookHash : public Common::UnaryFunction<HookHashKey, uint64> {
	uint64 operator()(HookHashKey val) const { return val.hash(); }
};

/** VM Hook mechanism */
class VmHooks {
public:
	VmHooks();

	/** Called just before executing opcode, to check if there is a requried hook */
	void vm_hook_before_exec(Sci::EngineState *s);

private:
	/** Hash map of all game's hooks */
	Common::HashMap<HookHashKey, HookEntry, HookHash> _hooksMap;
};


} // End of namespace Sci

#endif // SCI_ENGINE_VM_HOOKS_H
