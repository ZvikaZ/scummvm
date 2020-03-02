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



struct HookHashKey {
	SegmentId segment;
	uint32 offset;

	uint64 hash();

	bool operator==(const HookHashKey &other) const {
		return segment == other.segment && offset == other.offset;
	}


};

struct HookEntry {
	int scriptNumber;
	const char *objName;
	Common::String selector;
	const char *opcodeName;
};

struct HookHash : public Common::UnaryFunction<HookHashKey, uint64> {
	uint64 operator()(HookHashKey val) const { return val.hash(); }
};

class VmHooks {
public:
	VmHooks();

	// TODO - document
	void vm_hook_before_exec(Sci::EngineState *s);

	Common::HashMap<HookHashKey, HookEntry, HookHash> _hooksMap;
};


} // End of namespace Sci

#endif // SCI_ENGINE_VM_HOOKS_H
