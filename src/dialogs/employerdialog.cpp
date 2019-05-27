//  TimesheetsTracker is a desktop that aids you in tracking your timesheets
//  and seeing what work you have done.
//
//  Copyright(C) <2018> <Szymon Welgus>
//
//  This program is free software : you can redistribute it and /
//  or modify it under the terms of the GNU General Public License as published
//  by the Free Software Foundation
//  , either version 3 of the License
//  , or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful
//  , but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "employerdialog.h"

#include <wx/statline.h>

#include "../common/common.h"
#include "../common/ids.h"
#include "../common/util.h"
#include "../db/database_exception.h"
#include "../services/db_service.h"

namespace app::dialog
{
wxIMPLEMENT_DYNAMIC_CLASS(EmployerDialog, wxDialog);

wxBEGIN_EVENT_TABLE(EmployerDialog, wxDialog)
EVT_BUTTON(ids::ID_SAVE, EmployerDialog::OnSave)
EVT_BUTTON(wxID_CANCEL, EmployerDialog::OnCancel)
EVT_CHECKBOX(EmployerDialog::IDC_ISACTIVE, EmployerDialog::OnIsActiveCheck)
wxEND_EVENT_TABLE()

EmployerDialog::EmployerDialog(wxWindow* parent, bool isEdit, int employerId, const wxString& name)
    : mEmployerText(wxT(""))
    , bIsEdit(isEdit)
    , mEmployerId(employerId)
{
    long style = wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU;
    wxSize size;
    wxString title;
    if (bIsEdit) {
        title = wxT("Edit Employer");
        size.Set(330, 400);
    } else {
        title = wxT("Add Employer");
        size.Set(WIDTH, HEIGHT);
    }
    bool success = Create(parent, wxID_ANY, title, wxDefaultPosition, size, style, name);

    SetMinClientSize(wxSize(MIN_WIDTH, MIN_HEIGHT));
}

EmployerDialog::~EmployerDialog()
{
    Destroy();
}

void EmployerDialog::Launch()
{
    ShowModal();
}

bool EmployerDialog::Create(wxWindow* parent, wxWindowID windowId, const wxString& title, const wxPoint& point, const wxSize& size, long style, const wxString& name)
{
    bool created = wxDialog::Create(parent, windowId, title, point, size, style, name);
    if (created) {
        CreateControls();
        if (bIsEdit) {
            DataToControls();
        }

        GetSizer()->Fit(this);
        //SetIcon
        Centre();
    }

    return created;
}

void EmployerDialog::CreateControls()
{
    /* Window Sizing */
    auto mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    auto mainPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(mainPanelSizer, wxSizerFlags().Border(wxALL, 5));

    auto sizer = new wxBoxSizer(wxVERTICAL);
    mainPanelSizer->Add(sizer, 0);

    /* Employer Details Box */
    auto detailsBox = new wxStaticBox(this, wxID_ANY, wxT("Employer Details"));
    auto detailsBoxSizer = new wxStaticBoxSizer(detailsBox, wxVERTICAL);
    sizer->Add(detailsBoxSizer, common::sizers::ControlExpandProp);

    auto employerDetailsPanel = new wxPanel(this, wxID_STATIC);
    detailsBoxSizer->Add(employerDetailsPanel, common::sizers::ControlExpand);

    auto taskFlexGridSizer = new wxFlexGridSizer(0, 2, 0, 0);
    employerDetailsPanel->SetSizer(taskFlexGridSizer);

    /* --- Controls --- */
    /* Employer Name Control */
    auto employerName = new wxStaticText(employerDetailsPanel, wxID_STATIC, wxT("Name"));
    taskFlexGridSizer->Add(employerName, common::sizers::ControlCenterVertical);

    pEmployerCtrl = new wxTextCtrl(employerDetailsPanel, wxID_STATIC, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), wxTE_LEFT, wxDefaultValidator, wxT("employer_name_ctrl"));
    taskFlexGridSizer->Add(pEmployerCtrl, common::sizers::ControlDefault);

    if (bIsEdit) {
        auto isActiveFiller = new wxStaticText(employerDetailsPanel, wxID_STATIC, wxT(""));
        taskFlexGridSizer->Add(isActiveFiller, common::sizers::ControlDefault);

        /* Is Active Checkbox Control */
        pIsActiveCtrl = new wxCheckBox(employerDetailsPanel, IDC_ISACTIVE, wxT("Is Active"));
        taskFlexGridSizer->Add(pIsActiveCtrl, common::sizers::ControlDefault);

        /* Date Created Text Control */
        pDateCreatedTextCtrl = new wxStaticText(this, wxID_STATIC, wxT("Created on: %s"));
        auto font = pDateCreatedTextCtrl->GetFont();
        font.MakeItalic();
        font.SetPointSize(8);
        pDateCreatedTextCtrl->SetFont(font);
        detailsBoxSizer->Add(pDateCreatedTextCtrl, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

        /* Date Updated Text Control */
        pDateUpdatedTextCtrl = new wxStaticText(this, wxID_STATIC, wxT("Updated on: %s"));
        detailsBoxSizer->Add(pDateUpdatedTextCtrl, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    /* Horizontal Line*/
    auto separation_line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL, wxT("new_task_static_line"));
    mainSizer->Add(separation_line, 0, wxEXPAND | wxALL, 1);

    /* Button Panel */
    auto buttonPanel = new wxPanel(this, wxID_STATIC);
    auto buttonPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonPanel->SetSizer(buttonPanelSizer);
    mainSizer->Add(buttonPanel, common::sizers::ControlCenter);

    auto okButton = new wxButton(buttonPanel, ids::ID_SAVE, wxT("&Save"));
    auto cancelButton = new wxButton(buttonPanel, wxID_CANCEL, wxT("&Cancel"));

    buttonPanelSizer->Add(okButton, common::sizers::ControlDefault);
    buttonPanelSizer->Add(cancelButton, common::sizers::ControlDefault);
}

void EmployerDialog::DataToControls()
{
    services::db_service dbService;
    models::employer employer;
    try {
        employer = dbService.get_employer(mEmployerId);
    } catch (const db::database_exception& e) {
        // TODO Log exception
    }

    pEmployerCtrl->SetValue(employer.employer_name);

    wxString dateCreatedString = util::ConvertUnixTimestampToString(employer.date_created_utc);
    wxString dateCreatedLabel = pDateCreatedTextCtrl->GetLabelText();
    pDateCreatedTextCtrl->SetLabel(wxString::Format(dateCreatedLabel, dateCreatedString));

    wxString dateUpdatedString = util::ConvertUnixTimestampToString(employer.date_modified_utc);
    wxString dateUpdatedLabel = pDateUpdatedTextCtrl->GetLabelText();
    pDateUpdatedTextCtrl->SetLabel(wxString::Format(dateUpdatedLabel, dateUpdatedString));

    pIsActiveCtrl->SetValue(employer.is_active);
}

bool EmployerDialog::Validate()
{
    bool isInvalid = mEmployerText.length() > 255 || mEmployerText.length() < 2 ||
        mEmployerText.empty();
    if (isInvalid) {
        wxMessageBox(wxT("Employer name is invalid"), wxT("Validation failure"), wxOK | wxICON_EXCLAMATION);
        return false;
    }
    return true;
}

bool EmployerDialog::AreControlsEmpty()
{
    bool isEmpty = mEmployerText.empty();
    return isEmpty;
}

void EmployerDialog::OnSave(wxCommandEvent& event)
{
    mEmployerText = pEmployerCtrl->GetValue();

    bool validationSuccess = Validate();
    if (!validationSuccess) {
        return;
    }

    services::db_service dbService;
    try {
        if (bIsEdit && pIsActiveCtrl->IsChecked()) {
            models::employer employer;
            employer.employer_name = std::string(mEmployerText.ToUTF8());
            employer.date_modified_utc = util::UnixTimestamp();
            dbService.update_employer(employer);
        }
        if (bIsEdit && !pIsActiveCtrl->IsChecked()) {
            dbService.delete_employer(mEmployerId); // need to add date_modified
        }
        if (!bIsEdit) {
            dbService.create_new_employer(std::string(mEmployerText.ToUTF8()));
        }
    } catch (const db::database_exception& e) {
        wxMessageBox(wxT("An error occured\n"), wxT("Error"), wxOK_DEFAULT | wxICON_ERROR);
        return;
    }

    EndModal(ids::ID_SAVE);
}

void EmployerDialog::OnCancel(wxCommandEvent& event)
{
    bool areControlsEmpty = AreControlsEmpty();
    if (!areControlsEmpty) {

        int answer = wxMessageBox(wxT("Are you sure you want to cancel?"), wxT("Confirm"),
            wxYES_NO | wxICON_QUESTION);
        if (answer == wxYES) {
            EndModal(wxID_CANCEL);
        }
    } else {
        EndModal(wxID_CANCEL);
    }
}

void EmployerDialog::OnIsActiveCheck(wxCommandEvent& event)
{
    if (event.IsChecked()) {
        pEmployerCtrl->Enable();
    } else {
        pEmployerCtrl->Disable();
    }
}
}