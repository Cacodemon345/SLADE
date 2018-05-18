#pragma once

class MapPreviewCanvas;
class Archive;
class ZipArchive;
class ArchiveTreeNode;
class ListView;

class MapBackupPanel : public wxPanel
{
public:
	MapBackupPanel(wxWindow* parent);
	~MapBackupPanel() = default;

	Archive* selectedMapData() const { return archive_mapdata_.get(); }

	bool loadBackups(string archive_name, string_view map_name);
	void updateMapPreview();

private:
	MapPreviewCanvas*           canvas_map_   = nullptr;
	ListView*                   list_backups_ = nullptr;
	std::unique_ptr<ZipArchive> archive_backups_;
	std::unique_ptr<Archive>    archive_mapdata_;
	ArchiveTreeNode*            dir_current_ = nullptr;
};
