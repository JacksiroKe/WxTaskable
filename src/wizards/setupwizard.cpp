//  TimesheetsTracker is a desktop that aids you in tracking your timesheets
//  and seeing what work you have done.
//
//  Copyright(C)<2018><Szymon Welgus>
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
//
//
//  Contact:
//    szymonwelgus at gmail dot com

#include "setupwizard.h"

#include <wx/wx.h>

#include "../db/database_exception.h"
#include "../services/db_service.h"
#include "setup.xpm"

namespace app::wizard
{
SetupWizard::SetupWizard(wxFrame* frame)
    : wxWizard(frame, wxID_ANY, wxT("Setup Wizard"), wxBitmap(setupwizardxpm), wxDefaultPosition, wxDEFAULT_DIALOG_STYLE)
    , pPage1(nullptr)
    , mEmployer(wxGetEmptyString())
    , mEmployerId(0)
    , mClient(wxGetEmptyString())
    , mClientId(0)
    , mProject(wxGetEmptyString())
    , mProjectId(0)
{
    pPage1 = new wxWizardPageSimple(this);

    wxString introWizardMessage = wxT("This wizard will help you get started with Tasks Tracker.\n"
        "The next few pages will setup a employer, a client (which is optional), a project\n"
        " and a category\n. Please press \"Next\" to begin the process.");

    new wxStaticText(pPage1, wxID_ANY, introWizardMessage);

    auto page2 = new AddEmployerAndClientPage(this);
    // auto page3 = new AddProjectPage(this);
    // auto page4 = new AddCategoriesPage(this);

    wxWizardPageSimple::Chain(pPage1, page2);
    // wxWizardPageSimple::Chain(page2, page3);
    // wxWizardPageSimple::Chain(page3, page4);

    GetPageAreaSizer()->Add(pPage1);
}
bool SetupWizard::Run()
{
    auto wizardSuccess = wxWizard::RunWizard(pPage1);
    if (wizardSuccess) {
        // TODO
    }
    Destroy();
    return wizardSuccess;
}
wxString SetupWizard::GetEmployer() const
{
    return mEmployer;
}

void SetupWizard::SetEmployer(const wxString& employer)
{
    mEmployer = employer;
}

int SetupWizard::GetEmployerId() const
{
    return mEmployerId;
}

void SetupWizard::SetEmployerId(const int employerId)
{
    mEmployerId = employerId;
}

wxString SetupWizard::GetClient() const
{
    return mClient;
}

void SetupWizard::SetClient(const wxString& client)
{
    mClient = client;
}

int SetupWizard::GetClientId() const
{
    return mClientId;
}

void SetupWizard::SetClientId(const int clientId)
{
    mClientId = clientId;
}

wxString SetupWizard::GetProject() const
{
    return mProject;
}

void SetupWizard::SetProject(const wxString& project)
{
    mProject = project;
}

int SetupWizard::GetProjectId() const
{
    return mProjectId;
}

void SetupWizard::SetProjectId(const int projectId)
{
    mProjectId = projectId;
}

AddEmployerAndClientPage::AddEmployerAndClientPage(SetupWizard* parent)
    : wxWizardPageSimple(parent)
    , pParent(parent)
{
    auto sizer = new wxBoxSizer(wxVERTICAL);

    auto employerText = new wxStaticText(this, wxID_ANY, wxT("Employer:"));
    sizer->Add(employerText, 0, wxALL, 5);

    pEmployerCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pEmployerCtrl, 0, wxALL, 5);

    wxString employerHelpMessage = wxT("Specify a descriptive employer name.\n"
        "An employer is whoever you work for and under who all data will be grouped under");
    auto employerHelpText = new wxStaticText(this, wxID_ANY, employerHelpMessage);
    sizer->Add(employerHelpText, 0, wxALL, 5);

    auto clientText = new wxStaticText(this, wxID_ANY, wxT("Client:*"));
    sizer->Add(clientText, 0, wxALL, 5);

    pClientCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pClientCtrl, 0, wxALL, 5);

    wxString clientHelpMessage = wxT("Specify a descriptive name for a client.\n"
        "If your employer has multiple clients and you work with them then you can add a client\n"
        "A client is, however, optional and can be safely skipped if you do not deal with clients");
    auto clientHelpText = new wxStaticText(this, wxID_ANY, clientHelpMessage);
    sizer->Add(clientHelpText, 0, wxALL, 5);

    SetSizer(sizer);
    sizer->Fit(this);
}
bool AddEmployerAndClientPage::TransferDataFromWindow()
{
    const wxString employer = pEmployerCtrl->GetValue().Trim();
    if (employer.empty()) {
        wxMessageBox(wxT("An employer is required"), wxT("TasksTracker"), wxOK | wxICON_ERROR, this);
        return false;
    }

    services::db_service dbService;
    int employerId = 0;
    try {
        dbService.create_new_employer(std::string(employer.ToUTF8()));
        employerId = dbService.get_last_insert_rowid();
    } catch (const db::database_exception& e) {
        // TODO log exception
    }

    const wxString client = pClientCtrl->GetValue().Trim();
    int clientId = 0;
    if (!client.empty()) {
        try {
            dbService.create_new_client(std::string(client.ToUTF8()), employerId);
            clientId = dbService.get_last_insert_rowid();
        } catch (const db::database_exception& e) {
            // TODO log exception
        }
    }

    pParent->SetEmployer(employer);
    pParent->SetEmployerId(employerId);
    pParent->SetClient(client);
    pParent->SetClientId(clientId);

    return true;
}

AddProjectPage::AddProjectPage(SetupWizard* parent)
    : wxWizardPageSimple(parent)
    , pParent(parent)
{
    auto sizer = new wxBoxSizer(wxVERTICAL);

    // TODO message to say adding info to %employer% and %client%
    wxString infoMessage = wxGetEmptyString();
    if (pParent->GetClientId() != 0) {
        infoMessage = wxString::Format("Add a project for employer: %s and client: %s", pParent->GetEmployer(), pParent->GetClient());
    } else {
        infoMessage = wxString::Format("Add a project for employer: %s", pParent->GetEmployer());
    }

    auto infoText = new wxStaticText(this, wxID_ANY, infoMessage);
    sizer->Add(infoText, 0, wxALL, 5);

    auto projectText = new wxStaticText(this, wxID_ANY, wxT("Project:"));
    sizer->Add(projectText, 0, wxALL, 5);

    pNameCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pNameCtrl, 0, wxALL, 5);

    wxString projectNameHelpMessage = wxT("Specify a descriptive project name.\n"
        "A project is a undertaking of a business for a client or for itself carried out individually or in a group to achieve a business goal");
    auto projectNameHelpText = new wxStaticText(this, wxID_ANY, projectNameHelpMessage);
    sizer->Add(projectNameHelpText, 0, wxALL, 5);

    auto displayNameText = new wxStaticText(this, wxID_ANY, wxT("Display Name:"));
    sizer->Add(displayNameText, 0, wxALL, 5);

    pDisplayNameCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pDisplayNameCtrl, 0, wxALL, 5);

    wxString displayNameHelpMessage = wxT("Specify a shortened version of the project name.\n"
        "Similar to a project name, a display name is merely a shortened version of the project name to aid in readability, identification and display");
    auto displayNameHelpText = new wxStaticText(this, wxID_ANY, displayNameHelpMessage);
    sizer->Add(displayNameHelpText, 0, wxALL, 5);

    SetSizer(sizer);
    sizer->Fit(this);
}

bool AddProjectPage::TransferDataFromWindow()
{
    services::db_service dbService;

    const wxString projectName = pNameCtrl->GetValue().Trim();
    if (projectName.empty()) {
        wxMessageBox(wxT("An project name is required"), wxT("TasksTracker"), wxOK | wxICON_ERROR, this);
        return false;
    }

    const wxString displayName = pDisplayNameCtrl->GetValue().Trim();
    if (displayName.empty()) {
        wxMessageBox(wxT("An display name is required"), wxT("TasksTracker"), wxOK | wxICON_ERROR, this);
        return false;
    }

    int projectId = 0;
    try {
        bool isAssociatedWithClient = pParent->GetClientId() != 0;
        if (isAssociatedWithClient) {
            int clientId = pParent->GetClientId();
            dbService.create_new_project(std::string(projectName.ToUTF8()), std::string(displayName.ToUTF8()), pParent->GetEmployerId(), &clientId);
        } else {
            dbService.create_new_project(std::string(projectName.ToUTF8()), std::string(displayName.ToUTF8()), pParent->GetEmployerId(), nullptr);
        }
        projectId = dbService.get_last_insert_rowid();
    } catch (const db::database_exception& e) {
        // TODO log exception
    }

    pParent->SetProject(projectName);
    pParent->SetProjectId(projectId);

    return true;
}

AddCategoriesPage::AddCategoriesPage(SetupWizard* parent)
    : wxWizardPageSimple(parent)
    , pParent(parent)
{
    auto sizer = new wxBoxSizer(wxVERTICAL);

    auto infoMessage = wxString::Format("Add a category to the project: %s", pParent->GetProject());
    auto infoText = new wxStaticText(this, wxID_ANY, infoMessage);
    sizer->Add(infoText, 0, wxALL, 5);

    auto categoryText = new wxStaticText(this, wxID_ANY, wxT("Category:"));
    sizer->Add(categoryText, 0, wxALL, 5);

    pNameCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pNameCtrl, 0, wxALL, 5);

    wxString categoryNameHelpMessage = wxT("Specify a category for the project.\n"
        "A category is the specific type of task you worked on or did for said project, e.g. \"meetings\"");
    auto categoryNameHelpText = new wxStaticText(this, wxID_ANY, categoryNameHelpMessage);
    sizer->Add(categoryNameHelpText, 0, wxALL, 5);

    auto descriptionText = new wxStaticText(this, wxID_ANY, wxT("Description:"));
    sizer->Add(descriptionText, 0, wxALL, 5);

    pDescriptionCtrl = new wxTextCtrl(this, wxID_ANY, wxGetEmptyString(), wxDefaultPosition, wxSize(150, -1), 0);
    sizer->Add(pDescriptionCtrl, 0, wxALL, 5);

    wxString descriptionHelpMessage = wxT("Specify a description for the above category.\n"
        "A description for the category helps you create a distinction between similar categories for different projects");
    auto descriptionHelpText = new wxStaticText(this, wxID_ANY, descriptionHelpMessage);
    sizer->Add(descriptionHelpText, 0, wxALL, 5);

    SetSizer(sizer);
    sizer->Fit(this);
}

bool AddCategoriesPage::TransferDataFromWindow()
{
    services::db_service dbService;
    const wxString category = pNameCtrl->GetValue().Trim();
    if (category.empty()) {
        wxMessageBox(wxT("An category name is required"), wxT("TasksTracker"), wxOK | wxICON_ERROR, this);
        return false;
    }

    const wxString description = pDescriptionCtrl->GetValue().Trim();
    if (description.empty()) {
        wxMessageBox(wxT("An description is required"), wxT("TasksTracker"), wxOK | wxICON_ERROR, this);
        return false;
    }

    try {
        dbService.create_new_category(pParent->GetProjectId(), std::string(category.ToUTF8()), std::string(description.ToUTF8()));
    } catch (const db::database_exception& e) {
        // TODO log exception
    }

    return true;
}
}