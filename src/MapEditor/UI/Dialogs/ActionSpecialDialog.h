#pragma once

#include "UI/Controls/STabCtrl.h"
#include "UI/SDialog.h"
#include "UI/WxBasicControls.h"

namespace Game
{
struct ArgSpec;
}
class wxFlexGridSizer;
class ArgsControl;
class GenLineSpecialPanel;
class MapObject;
class NumberTextCtrl;

// A wxDataViewTreeCtrl specialisation showing the
// action specials and groups in a tree structure
class ActionSpecialTreeView : public wxDataViewTreeCtrl
{
public:
	ActionSpecialTreeView(wxWindow* parent);
	~ActionSpecialTreeView() = default;

	void setParentDialog(wxDialog* dlg) { parent_dialog_ = dlg; }

	int  specialNumber(wxDataViewItem item) const;
	void showSpecial(int special, bool focus = true);
	int  selectedSpecial() const;

	void onItemEdit(wxDataViewEvent& e);
	void onItemActivated(wxDataViewEvent& e);

private:
	wxDataViewItem root_;
	wxDataViewItem item_none_;
	wxDialog*      parent_dialog_ = nullptr;

	// It's incredibly retarded that I actually have to do this
	struct ASTVGroup
	{
		string         name;
		wxDataViewItem item;
		ASTVGroup(wxDataViewItem i, string_view name) : item{ i }, name{ name.data(), name.size() } {}
	};
	vector<ASTVGroup> groups_;

	wxDataViewItem getGroup(string_view group);
};

class ArgsPanel : public wxScrolled<wxPanel>
{
public:
	ArgsPanel(wxWindow* parent);
	~ArgsPanel() = default;

	void setup(const Game::ArgSpec& args, bool udmf);
	void setValues(int args[5]);
	int  argValue(int index);

private:
	wxFlexGridSizer* fg_sizer_           = nullptr;
	ArgsControl*     control_args_[5]    = { nullptr, nullptr, nullptr, nullptr, nullptr };
	wxStaticText*    label_args_[5]      = { nullptr, nullptr, nullptr, nullptr, nullptr };
	wxStaticText*    label_args_desc_[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	void onSize(wxSizeEvent& event);
};

class ActionSpecialPanel : public wxPanel
{
public:
	ActionSpecialPanel(wxWindow* parent, bool trigger = true);
	~ActionSpecialPanel() = default;

	void setupSpecialPanel();
	void setArgsPanel(ArgsPanel* panel) { panel_args_ = panel; }
	void setSpecial(int special);
	void setTrigger(int index);
	void setTrigger(string_view trigger);
	void clearTrigger();
	int  selectedSpecial() const;
	void showGeneralised(bool show = true);
	void applyTo(vector<MapObject*>& lines, bool apply_special);
	void openLines(vector<MapObject*>& lines);

private:
	ActionSpecialTreeView* tree_specials_        = nullptr;
	wxPanel*               panel_action_special_ = nullptr;
	GenLineSpecialPanel*   panel_gen_specials_   = nullptr;
	wxRadioButton*         rb_special_           = nullptr;
	wxRadioButton*         rb_generalised_       = nullptr;
	ArgsPanel*             panel_args_           = nullptr;
	wxChoice*              choice_trigger_       = nullptr;
	bool                   show_trigger_         = true;
	NumberTextCtrl*        text_special_         = nullptr;
	wxButton*              btn_preset_           = nullptr;

	struct FlagHolder
	{
		wxCheckBox* check_box;
		int         index;
		string      udmf;
	};
	vector<FlagHolder> flags_;

	void onRadioButtonChanged(wxCommandEvent& e);
	void onSpecialSelectionChanged(wxDataViewEvent& e);
	void onSpecialItemActivated(wxDataViewEvent& e);
	void onSpecialPresetClicked(wxCommandEvent& e);
};

class ActionSpecialDialog : public SDialog
{
public:
	ActionSpecialDialog(wxWindow* parent, bool show_args = false);
	~ActionSpecialDialog() = default;

	void setSpecial(int special) const;
	void setArgs(int args[5]) const;
	int  selectedSpecial() const;
	int  arg(int index) const;
	void applyTo(vector<MapObject*>& lines, bool apply_special) const;
	void openLines(vector<MapObject*>& lines) const;

private:
	ActionSpecialPanel* panel_special_ = nullptr;
	ArgsPanel*          panel_args_    = nullptr;
	TabControl*         stc_tabs_      = nullptr;
};
