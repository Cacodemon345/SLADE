#pragma once

#include "General/UndoRedo.h"
#include "MapEditor/SLADEMap/MapObject.h"

namespace MapEditor
{
// UndoStep for when a MapObject has properties changed
class PropertyChangeUS : public UndoStep
{
public:
	PropertyChangeUS(MapObject* object);
	~PropertyChangeUS() = default;

	void doSwap(MapObject* obj);
	bool doUndo() override;
	bool doRedo() override;

private:
	std::unique_ptr<MapObject::Backup> backup_;
};

// UndoStep for when a MapObject is either created or deleted
class MapObjectCreateDeleteUS : public UndoStep
{
public:
	MapObjectCreateDeleteUS();
	~MapObjectCreateDeleteUS() = default;

	static bool isValid(vector<unsigned>& list) { return !(list.size() == 1 && list[0] == 0); }
	void        swapLists();
	bool        doUndo() override;
	bool        doRedo() override;
	void        checkChanges();
	bool        isOk() override;

private:
	vector<unsigned> vertices_;
	vector<unsigned> lines_;
	vector<unsigned> sides_;
	vector<unsigned> sectors_;
	vector<unsigned> things_;
};

// UndoStep for when multiple MapObjects have properties changed
class MultiMapObjectPropertyChangeUS : public UndoStep
{
public:
	MultiMapObjectPropertyChangeUS();
	~MultiMapObjectPropertyChangeUS() = default;

	void doSwap(MapObject* obj, unsigned index);
	bool doUndo() override;
	bool doRedo() override;
	bool isOk() override { return !backups_.empty(); }

private:
	vector<std::unique_ptr<MapObject::Backup>> backups_;
};
} // namespace MapEditor
