// Productivity tool to help you track the time you spend on tasks
// Copyright (C) 2020  Szymon Welgus
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  Contact:
//    szymonwelgus at gmail dot com

#include "mainframe.h"

#include <vector>

#include <sqlite_modern_cpp/errors.h>

#include <wx/aboutdlg.h>
#include <wx/clipbrd.h>
#include <wx/taskbarbutton.h>

#include "../common/constants.h"
#include "../common/common.h"
#include "../common/ids.h"
#include "../common/util.h"
#include "../common/version.h"

#include "../data/taskitemdata.h"
#include "../models/taskitemmodel.h"

#include "../dialogs/taskitemdlg.h"
#include "../dialogs/employerdlg.h"
#include "../dialogs/clientdlg.h"
#include "../dialogs/projectdlg.h"
#include "../dialogs/categorydlg.h"
#include "../dialogs/editlistdlg.h"
#include "../dialogs/stopwatchtaskdlg.h"
#include "../dialogs/checkforupdatedlg.h"
#include "../dialogs/categoriesdlg.h"

#include "../dialogs/preferencesdlg.h"

#include "../wizards/databaserestorewizard.h"
#include "taskbaricon.h"

#include "../services/databaseconnection.h"
#include "../services/databasebackup.h"
#include "../services/databasebackupdeleter.h"

namespace app::frm
{
// clang-format off
wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
EVT_CLOSE(MainFrame::OnClose)
EVT_MENU(wxID_EXIT, MainFrame::OnExit)
EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
EVT_MENU(ids::ID_NEW_ENTRY_TASK, MainFrame::OnNewEntryTask)
EVT_MENU(ids::ID_NEW_TIMED_TASK, MainFrame::OnNewTimedTask)
EVT_MENU(ids::ID_NEW_EMPLOYER, MainFrame::OnNewEmployer)
EVT_MENU(ids::ID_NEW_PROJECT, MainFrame::OnNewProject)
EVT_MENU(ids::ID_NEW_CLIENT, MainFrame::OnNewClient)
EVT_MENU(ids::ID_NEW_CATEGORY, MainFrame::OnNewCategory)
EVT_MENU(ids::ID_EDIT_EMPLOYER, MainFrame::OnEditEmployer)
EVT_MENU(ids::ID_EDIT_CLIENT, MainFrame::OnEditClient)
EVT_MENU(ids::ID_EDIT_PROJECT, MainFrame::OnEditProject)
EVT_MENU(ids::ID_EDIT_CATEGORY, MainFrame::OnEditCategory)
EVT_MENU(ids::ID_PREFERENCES, MainFrame::OnPreferences)
EVT_MENU(ids::ID_STOPWATCH_TASK, MainFrame::OnTaskStopwatch)
EVT_MENU(ids::ID_CHECK_FOR_UPDATE, MainFrame::OnCheckForUpdate)
EVT_MENU(ids::ID_RESTORE_DATABASE, MainFrame::OnRestoreDatabase)
EVT_MENU(ids::ID_BACKUP_DATABASE, MainFrame::OnBackupDatabase)
EVT_LIST_ITEM_ACTIVATED(MainFrame::IDC_LIST, MainFrame::OnItemDoubleClick)
EVT_LIST_ITEM_RIGHT_CLICK(MainFrame::IDC_LIST, MainFrame::OnItemRightClick)
EVT_COMMAND(wxID_ANY, EVT_TASK_ITEM_INSERTED, MainFrame::OnTaskInserted)
EVT_COMMAND(wxID_ANY, START_NEW_STOPWATCH_TASK, MainFrame::OnNewStopwatchTaskFromPausedStopwatchTask)
EVT_ICONIZE(MainFrame::OnIconize)
EVT_DATE_CHANGED(MainFrame::IDC_GO_TO_DATE, MainFrame::OnDateChanged)
EVT_SIZE(MainFrame::OnResize)
EVT_BUTTON(MainFrame::IDC_FEEDBACK, MainFrame::OnFeedback)
EVT_CHAR_HOOK(MainFrame::OnKeyDown)
EVT_TIMER(IDC_DISMISS_INFOBAR_TIMER, MainFrame::OnDismissInfoBar)
EVT_MENU(wxID_COPY, MainFrame::OnPopupMenuCopyToClipboard)
EVT_MENU(wxID_EDIT, MainFrame::OnPopupMenuEdit)
EVT_MENU(wxID_DELETE, MainFrame::OnPopupMenuDelete)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(std::shared_ptr<cfg::Configuration> config,
    std::shared_ptr<spdlog::logger> logger,
    sqlite::database* database,
    const wxString& name)
    :wxFrame(
        nullptr, wxID_ANY, common::GetProgramName(), wxDefaultPosition, wxSize(600, 500), wxDEFAULT_FRAME_STYLE, name)
    , pConfig(config)
    , pLogger(logger)
    , pDatabase(database)
    , pTaskState(std::make_shared<services::TaskStateService>())
    , pTaskStorage(std::make_unique<services::TaskStorage>())
    , pDismissInfoBarTimer(std::make_unique<wxTimer>(this, IDC_DISMISS_INFOBAR_TIMER))
    , pDatePickerCtrl(nullptr)
    , pTotalHoursText(nullptr)
    , pListCtrl(nullptr)
    , pStatusBar(nullptr)
    , pInfoBar(nullptr)
    , pTaskBarIcon(nullptr)
    , bHasPendingTaskToResume(false)
    , pFeedbackButton(nullptr)
    , pFeedbackPopupWindow(nullptr)
    , mItemIndexForClipboard(-1)
    , mSelectedTaskItemId(-1)
// clang-format on
{
    svc::DatabaseConnection::Get().SetHandle(pDatabase);
}

MainFrame::~MainFrame()
{
    auto size = GetSize();
    pConfig->SetFrameSize(size);

    if (pTaskBarIcon) {
        delete pTaskBarIcon;
    }

    RunDatabaseBackup();

    if (pDatabase) {
        delete pDatabase;
        pDatabase = nullptr;
    }
}

bool MainFrame::CreateFrame()
{
    auto configSize = pConfig->GetFrameSize();
    SetSize(configSize);

    bool success = Create();
    SetMinClientSize(wxSize(599, 499));
    SetIcon(common::GetProgramIcon());

    if (pConfig->IsBackupEnabled()) {
        svc::DatabaseBackupDeleter dbBackupDeleter(pConfig);
        dbBackupDeleter.Execute();
    }

    pTaskBarIcon = new TaskBarIcon(this, pConfig, pLogger, pDatabase);
    if (pConfig->IsShowInTray()) {
        pTaskBarIcon->SetTaskBarIcon();
    }

    return success;
}

void MainFrame::ResetDatabaseHandleOnDatabaseRestore(sqlite::database* database)
{
    pDatabase = database;
    svc::DatabaseConnection::Get().ResetHandle(database);
}

bool MainFrame::Create()
{
    CreateControls();
    DataToControls();

    return true;
}

void MainFrame::CreateControls()
{
    /* Status Bar Control */
    int statusBarWidths[] = { 128, -1, 36 };

    pStatusBar = CreateStatusBar(3);
    SetStatusWidths(3, statusBarWidths);

    SetStatusText(wxT("Ready"), 0);
    SetStatusText(wxString::Format("%d.%d.%d", TASKABLE_MAJOR, TASKABLE_MINOR, TASKABLE_PATCH), 1);

    wxRect statusBarRect;
    pStatusBar->GetFieldRect(2, statusBarRect);

    pFeedbackButton = new wxBitmapButton(pStatusBar,
        IDC_FEEDBACK,
        common::GetFeedbackIcon(),
        statusBarRect.GetPosition(),
        wxSize(32, 20),
        wxBU_LEFT | wxBU_RIGHT);

    /* File Menu Control */
    auto fileMenu = new wxMenu();

    auto entryTaskMenuItem =
        fileMenu->Append(ids::ID_NEW_ENTRY_TASK, wxT("New &Entry Task\tCtrl-E"), wxT("Create new entry task"));
    entryTaskMenuItem->SetBitmap(common::GetEntryTaskIcon());

    auto timedTaskMenuItem =
        fileMenu->Append(ids::ID_NEW_TIMED_TASK, wxT("New &Timed Task\tCtrl-T"), wxT("Create new timed task"));
    timedTaskMenuItem->SetBitmap(common::GetTimedTaskIcon());

    fileMenu->AppendSeparator();

    auto stopwatchMenuItem =
        fileMenu->Append(ids::ID_STOPWATCH_TASK, wxT("Stop&watch\tCtrl-W"), wxT("Start task stopwatch"));
    stopwatchMenuItem->SetBitmap(common::GetStopwatchIcon());

    fileMenu->AppendSeparator();
    fileMenu->Append(ids::ID_NEW_EMPLOYER, wxT("New &Employer"), wxT("Create new employer"));
    fileMenu->Append(ids::ID_NEW_CLIENT, wxT("New &Client"), wxT("Create new client"));
    fileMenu->Append(ids::ID_NEW_PROJECT, wxT("New &Project"), wxT("Create new project"));
    fileMenu->Append(ids::ID_NEW_CATEGORY, wxT("New C&ategory"), wxT("Create new category"));
    fileMenu->AppendSeparator();
    auto exitMenuItem = fileMenu->Append(wxID_EXIT, wxT("Exit"), wxT("Exit the application"));
    exitMenuItem->SetBitmap(common::GetQuitIcon());

    /* Edit Menu Control */
    auto editMenu = new wxMenu();
    editMenu->Append(ids::ID_EDIT_EMPLOYER, wxT("Edit &Employer"), wxT("Select a employer to edit"));
    editMenu->Append(ids::ID_EDIT_CLIENT, wxT("Edit &Client"), wxT("Select a client to edit"));
    editMenu->Append(ids::ID_EDIT_PROJECT, wxT("Edit &Project"), wxT("Select a project to edit"));
    editMenu->Append(ids::ID_EDIT_CATEGORY, wxT("Edit C&ategory"), wxT("Select a category to edit"));
    editMenu->AppendSeparator();
    auto preferencesMenuItem =
        editMenu->Append(ids::ID_PREFERENCES, wxT("&Preferences\tCtrl-P"), wxT("Edit application preferences"));
    preferencesMenuItem->SetBitmap(common::GetSettingsIcon());

    /* Export Menu Control */
    // auto exportMenu = new wxMenu();

    /* Tools Menu Control */
    auto toolsMenu = new wxMenu();
    auto restoreMenuItem = toolsMenu->Append(
        ids::ID_RESTORE_DATABASE, wxT("Restore Database"), wxT("Restore database to a previous point"));
    restoreMenuItem->SetBitmap(common::GetDatabaseRestoreIcon());
    auto backupMenuItem = toolsMenu->Append(
        ids::ID_BACKUP_DATABASE, wxT("Backup Database"), wxT("Backup database at the current snapshot"));
    backupMenuItem->SetBitmap(common::GetDatabaseBackupIcon());

    /* Help Menu Control */
    wxMenu* helpMenu = new wxMenu();
    auto helpMenuItem = helpMenu->Append(wxID_ABOUT);
    helpMenuItem->SetBitmap(common::GetAboutIcon());
    auto checkUpdateMenuItem = helpMenu->Append(
        ids::ID_CHECK_FOR_UPDATE, wxT("Check for update"), wxT("Check if an update is available for application"));
    checkUpdateMenuItem->SetBitmap(common::GetCheckForUpdateIcon());

    /* Menu Bar */
    wxMenuBar* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, wxT("File"));
    menuBar->Append(editMenu, wxT("Edit"));
    // menuBar->Append(exportMenu, wxT("Export"));
    menuBar->Append(toolsMenu, wxT("Tools"));
    menuBar->Append(helpMenu, wxT("Help"));

    SetMenuBar(menuBar);

    /* Accelerator Table */
    wxAcceleratorEntry entries[4];
    entries[0].Set(wxACCEL_CTRL, (int) 'N', ids::ID_NEW_ENTRY_TASK);
    entries[1].Set(wxACCEL_CTRL, (int) 'T', ids::ID_NEW_TIMED_TASK);
    entries[2].Set(wxACCEL_CTRL, (int) 'P', ids::ID_PREFERENCES);
    entries[3].Set(wxACCEL_CTRL, (int) 'W', ids::ID_STOPWATCH_TASK);

    wxAcceleratorTable table(ARRAYSIZE(entries), entries);
    SetAcceleratorTable(table);

    /* Main Controls */
    auto mainSizer = new wxBoxSizer(wxVERTICAL);
    auto mainPanel = new wxPanel(this);
    mainPanel->SetSizer(mainSizer);

    /* InfoBar Control */
    pInfoBar = new wxInfoBar(mainPanel, wxID_ANY);
    mainSizer->Add(pInfoBar, wxSizerFlags().Expand());

    /* Utilities Panel and Controls*/
    auto utilPanel = new wxPanel(mainPanel);
    mainSizer->Add(utilPanel);
    auto utilSizer = new wxBoxSizer(wxHORIZONTAL);
    utilPanel->SetSizer(utilSizer);

    auto gotoText = new wxStaticText(utilPanel, wxID_ANY, wxT("Go To"));
    utilSizer->Add(gotoText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    pDatePickerCtrl = new wxDatePickerCtrl(
        utilPanel, IDC_GO_TO_DATE, wxDefaultDateTime, wxDefaultPosition, wxSize(150, -1), wxDP_DROPDOWN);
    pDatePickerCtrl->SetToolTip(wxT("Select a date to navigate to"));
    utilSizer->Add(pDatePickerCtrl, common::sizers::ControlDefault);

    pTotalHoursText = new wxStaticText(utilPanel, IDC_HOURS_TEXT, wxT("Total Hours: %d"));
    pTotalHoursText->SetToolTip(wxT("Indicates the total hours spent on tasks for the selected day"));
    utilSizer->AddStretchSpacer();
    utilSizer->Add(pTotalHoursText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    /* List Panel and Control*/
    auto listPanel = new wxPanel(mainPanel);
    mainSizer->Add(listPanel, 1, wxEXPAND);
    auto listSizer = new wxBoxSizer(wxHORIZONTAL);
    listPanel->SetSizer(listSizer);

    pListCtrl = new wxListCtrl(
        listPanel, IDC_LIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    pListCtrl->SetFocus();
    listSizer->Add(pListCtrl, 1, wxEXPAND | wxALL, 5);

    wxListItem projectColumn;
    projectColumn.SetId(0);
    projectColumn.SetText(wxT("Project"));
    pListCtrl->InsertColumn(0, projectColumn);

    wxListItem dateColumn;
    dateColumn.SetId(1);
    dateColumn.SetText(wxT("Date"));
    pListCtrl->InsertColumn(1, dateColumn);

    wxListItem startTimeColumn;
    startTimeColumn.SetId(2);
    startTimeColumn.SetText(wxT("Started"));
    pListCtrl->InsertColumn(2, startTimeColumn);

    wxListItem endTimeColumn;
    endTimeColumn.SetId(3);
    endTimeColumn.SetText(wxT("Ended"));
    pListCtrl->InsertColumn(3, endTimeColumn);

    wxListItem durationColumn;
    durationColumn.SetId(4);
    durationColumn.SetText(wxT("Duration"));
    pListCtrl->InsertColumn(4, durationColumn);

    wxListItem categoryColumn;
    categoryColumn.SetId(5);
    categoryColumn.SetText(wxT("Category"));
    pListCtrl->InsertColumn(5, categoryColumn);

    wxListItem descriptionColumn;
    descriptionColumn.SetId(6);
    descriptionColumn.SetText(wxT("Description"));
    pListCtrl->InsertColumn(6, descriptionColumn);
}

void MainFrame::DataToControls()
{
    CalculateTotalTime();
    RefreshItems();
}

void MainFrame::OnAbout(wxCommandEvent& event)
{
    wxAboutDialogInfo aboutInfo;
    aboutInfo.SetIcon(common::GetProgramIcon64());
    aboutInfo.SetName(common::GetProgramName());
    aboutInfo.SetVersion(wxString::Format("%d.%d.%d", TASKABLE_MAJOR, TASKABLE_MINOR, TASKABLE_PATCH));
    aboutInfo.SetDescription(wxT("A desktop application to help you manage how you've spent\n"
                                 "your time on tasks during the day by tracking the time\n"
                                 "you've spent on those tasks throughout the day"));
    aboutInfo.SetCopyright("(C) 2018-2020");
    aboutInfo.SetWebSite(wxT("https://github.com/ifexception/taskable"));
    aboutInfo.SetLicence(common::GetLicense());
    aboutInfo.AddDeveloper(wxT("Szymon Welgus"));

    wxAboutBox(aboutInfo);
}

void MainFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    if (pConfig->IsConfirmOnExit() && event.CanVeto()) {
        int ret = wxMessageBox(
            wxT("Are you sure to exit the application?"), common::GetProgramName(), wxICON_QUESTION | wxYES_NO);
        if (ret == wxNO) {
            event.Veto();
            return;
        }
    } else if (pConfig->IsCloseToTray() && pConfig->IsShowInTray() && event.CanVeto()) {
        Hide();
        MSWGetTaskBarButton()->Hide();
        return;
    }
    event.Skip();
}

void MainFrame::OnNewEntryTask(wxCommandEvent& event)
{
    wxDateTime date = pDatePickerCtrl->GetValue();
    dlg::TaskItemDialog entryTask(this, pLogger, pConfig, constants::TaskItemTypes::EntryTask, date);
    int retCode = entryTask.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("task"));
}

void MainFrame::OnNewTimedTask(wxCommandEvent& event)
{
    wxDateTime date = pDatePickerCtrl->GetValue();
    dlg::TaskItemDialog timedTask(this, pLogger, pConfig, constants::TaskItemTypes::TimedTask, date);
    int retCode = timedTask.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("task"));
}

void MainFrame::OnNewEmployer(wxCommandEvent& event)
{
    dlg::EmployerDialog newEmployer(this, pLogger);
    int retCode = newEmployer.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("employer"));
}

void MainFrame::OnNewClient(wxCommandEvent& event)
{
    dlg::ClientDialog newClient(this, pLogger);
    int retCode = newClient.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("client"));
}

void MainFrame::OnNewProject(wxCommandEvent& event)
{
    dlg::ProjectDialog newProject(this, pLogger);
    int retCode = newProject.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("project"));
}

void MainFrame::OnNewCategory(wxCommandEvent& event)
{
    dlg::CategoriesDialog categoriesDialog(this, pLogger);
    int retCode = categoriesDialog.ShowModal();
    ShowInfoBarMessageForAdd(retCode, wxT("categories"));
}

void MainFrame::OnEditEmployer(wxCommandEvent& event)
{
    dlg::EditListDialog employerEdit(this, dlg::DialogType::Employer, pLogger);
    int retCode = employerEdit.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("employer"));
}

void MainFrame::OnEditClient(wxCommandEvent& event)
{
    dlg::EditListDialog clientEdit(this, dlg::DialogType::Client, pLogger);
    int retCode = clientEdit.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("client"));
}

void MainFrame::OnEditProject(wxCommandEvent& event)
{
    dlg::EditListDialog projectEdit(this, dlg::DialogType::Project, pLogger);
    int retCode = projectEdit.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("project"));
}

void MainFrame::OnEditCategory(wxCommandEvent& event)
{
    dlg::EditListDialog categoryEdit(this, dlg::DialogType::Category, pLogger);
    int retCode = categoryEdit.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("category"));
}

void MainFrame::OnTaskInserted(wxCommandEvent& event)
{
    pListCtrl->DeleteAllItems();

    auto selectedDate = pDatePickerCtrl->GetValue();

    CalculateTotalTime(selectedDate);
    RefreshItems(selectedDate);
}

void MainFrame::OnItemDoubleClick(wxListEvent& event)
{
    auto taskItemId = event.GetData();

    data::TaskItemData taskItemData;
    int taskItemTypeId = 0;
    try {
        taskItemTypeId = taskItemData.GetTaskItemTypeIdByTaskItemId(taskItemId);
    } catch (const sqlite::sqlite_exception& e) {
        pLogger->error(
            "Error occured on TaskItemData::GetTaskItemTypeIdByTaskItemId() - {0:d} : {1}", e.get_code(), e.what());
        return;
    }
    constants::TaskItemTypes type = static_cast<constants::TaskItemTypes>(taskItemTypeId);
    wxDateTime dateContext = pDatePickerCtrl->GetValue();

    dlg::TaskItemDialog editTask(this, pLogger, pConfig, type, true, taskItemId, dateContext);
    int retCode = editTask.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("task"));
}

void MainFrame::OnItemRightClick(wxListEvent& event)
{
    mItemIndexForClipboard = event.GetIndex();
    mSelectedTaskItemId = event.GetData();

    wxMenu menu;

    menu.Append(wxID_COPY, wxT("&Copy to Clipboard"));
    menu.Append(wxID_EDIT, wxT("&Edit"));
    menu.Append(wxID_DELETE, wxT("&Delete"));

    PopupMenu(&menu);
}

void MainFrame::OnIconize(wxIconizeEvent& event)
{
    if (event.IsIconized() && pConfig->IsShowInTray() && pConfig->IsMinimizeToTray()) {
        MSWGetTaskBarButton()->Hide();
    }
}

void MainFrame::OnPreferences(wxCommandEvent& event)
{
    dlg::PreferencesDialog preferences(this, pConfig, pLogger, pTaskBarIcon);
    preferences.ShowModal();
}

void MainFrame::OnTaskStopwatch(wxCommandEvent& event)
{
    dlg::StopwatchTaskDialog stopwatchTask(this, pConfig, pLogger, pTaskState, pTaskBarIcon);
    stopwatchTask.Launch();

    pListCtrl->SetFocus();
}

void MainFrame::OnDateChanged(wxDateEvent& event)
{
    pListCtrl->DeleteAllItems();

    auto date = event.GetDate();

    CalculateTotalTime(date);
    RefreshItems(date);

    pListCtrl->SetFocus();
}

void MainFrame::OnNewStopwatchTaskFromPausedStopwatchTask(wxCommandEvent& event)
{
    pTaskStorage->Store(pTaskState);
    pTaskState->mTimes.clear();

    dlg::StopwatchTaskDialog stopwatchTask(
        this, pConfig, pLogger, pTaskState, pTaskBarIcon, /* hasPendingPausedTask */ true);
    stopwatchTask.Launch();

    pTaskState->mTimes.clear();
    pTaskStorage->Restore(pTaskState);

    dlg::StopwatchTaskDialog stopwatchPausedTask(this, pConfig, pLogger, pTaskState, pTaskBarIcon);
    stopwatchPausedTask.Relaunch();

    pTaskStorage->mTimes.clear();

    pListCtrl->SetFocus();
}

void MainFrame::OnCheckForUpdate(wxCommandEvent& event)
{
    dlg::CheckForUpdateDialog checkForUpdate(this);
    checkForUpdate.LaunchModal();
}

void MainFrame::OnResize(wxSizeEvent& event)
{
    auto frameSize = GetClientSize();
    int width = frameSize.GetWidth();
    if (pListCtrl) {
        pListCtrl->SetColumnWidth(0, width * 0.10); // project
        pListCtrl->SetColumnWidth(1, width * 0.11); // task date
        pListCtrl->SetColumnWidth(2, width * 0.09); // start time
        pListCtrl->SetColumnWidth(3, width * 0.09); // end time
        pListCtrl->SetColumnWidth(4, width * 0.10); // duration
        pListCtrl->SetColumnWidth(5, width * 0.12); // category
        pListCtrl->SetColumnWidth(6, width * 0.37); // description
    }

    if (pStatusBar) {
        wxRect statusBarRect;
        pStatusBar->GetFieldRect(2, statusBarRect);
        pFeedbackButton->SetPosition(statusBarRect.GetPosition());
    }

    event.Skip();
}

void MainFrame::OnRestoreDatabase(wxCommandEvent& event)
{
    if (pConfig->IsBackupEnabled()) {
        auto wizard = new wizard::DatabaseRestoreWizard(this, pConfig, pLogger, pDatabase);
        wizard->CenterOnParent();
        bool wizardSetupSuccess = wizard->Run();
        if (wizardSetupSuccess) {
            pListCtrl->DeleteAllItems();
            RefreshItems();
        }
    } else {
        wxMessageBox(wxT("Error! Backup option is turned off\n"
                         "and database cannot be restored."),
            common::GetProgramName(),
            wxICON_WARNING | wxOK_DEFAULT);
    }
}

void MainFrame::OnBackupDatabase(wxCommandEvent& event)
{
    if (pConfig->IsBackupEnabled()) {
        svc::DatabaseBackup databaseBackup(pConfig, pLogger, pDatabase);
        bool result = databaseBackup.Execute();
        if (result) {
            wxMessageBox(
                wxT("Backup completed successfully!"), common::GetProgramName(), wxOK_DEFAULT | wxICON_INFORMATION);
        } else {
            wxMessageBox(wxT("Backup database operation encountered error(s)!"),
                common::GetProgramName(),
                wxOK_DEFAULT | wxICON_ERROR);
        }
    } else {
        wxMessageBox(
            wxT("Error! Backup option is turned off"), common::GetProgramName(), wxICON_WARNING | wxOK_DEFAULT);
    }
}

void MainFrame::OnFeedback(wxCommandEvent& event)
{
    delete pFeedbackPopupWindow;
    pFeedbackPopupWindow = new FeedbackPopupWindow(this);

    wxWindow* btn = (wxWindow*) event.GetEventObject();
    wxPoint pos = btn->ClientToScreen(wxPoint(-186, -78));
    wxSize sz = btn->GetSize();
    pFeedbackPopupWindow->Position(pos, sz);
    pFeedbackPopupWindow->Popup(nullptr);
}

void MainFrame::OnKeyDown(wxKeyEvent& event)
{
    pListCtrl->DeleteAllItems();
    auto currentDateTime = pDatePickerCtrl->GetValue();

    if (event.GetKeyCode() == WXK_RIGHT) {
        currentDateTime.Add(wxDateSpan::Days(1));
    }
    if (event.GetKeyCode() == WXK_LEFT) {
        currentDateTime.Add(wxDateSpan::Days(-1));
    }

    pDatePickerCtrl->SetValue(currentDateTime);
    RefreshItems(currentDateTime);
    CalculateTotalTime(currentDateTime);

    event.Skip();
}

void MainFrame::OnDismissInfoBar(wxTimerEvent& event)
{
    pDismissInfoBarTimer->Stop();
    pInfoBar->Dismiss();
}

void MainFrame::OnPopupMenuCopyToClipboard(wxCommandEvent& event)
{
    auto canOpen = wxTheClipboard->Open();
    if (canOpen) {
        wxListItem listItem;
        listItem.m_itemId = mItemIndexForClipboard;
        listItem.m_col = 6;
        listItem.m_mask = wxLIST_MASK_TEXT;
        pListCtrl->GetItem(listItem);

        auto textData = new wxTextDataObject(listItem.GetText());
        wxTheClipboard->SetData(textData);
        wxTheClipboard->Close();
    }

    mItemIndexForClipboard = -1;
}

void MainFrame::OnPopupMenuEdit(wxCommandEvent& event)
{
    data::TaskItemData taskItemData;

    int taskItemTypeId = 0;
    try {
        taskItemTypeId = taskItemData.GetTaskItemTypeIdByTaskItemId(mSelectedTaskItemId);
    } catch (const sqlite::sqlite_exception& e) {
        pLogger->error(
            "Error occured on TaskItemData::GetTaskItemTypeIdByTaskItemId() - {0:d} : {1}", e.get_code(), e.what());
        return;
    }
    constants::TaskItemTypes type = static_cast<constants::TaskItemTypes>(taskItemTypeId);
    wxDateTime dateContext = pDatePickerCtrl->GetValue();

    dlg::TaskItemDialog editTask(this, pLogger, pConfig, type, true, mSelectedTaskItemId, dateContext);
    int retCode = editTask.ShowModal();
    ShowInfoBarMessageForEdit(retCode, wxT("task"));
}

void MainFrame::OnPopupMenuDelete(wxCommandEvent& event)
{
    data::TaskItemData taskItemData;

    try {
        taskItemData.Delete(mSelectedTaskItemId);
    } catch (const sqlite::sqlite_exception& e) {
        pLogger->error("Error occured on TaskItemData::Delete() - {0:d} : {1}", e.get_code(), e.what());
        ShowInfoBarMessageForDelete(false);
        return;
    }

    ShowInfoBarMessageForDelete(true);
}

void MainFrame::CalculateTotalTime(wxDateTime date)
{
    auto dateString = date.FormatISODate();

    data::TaskItemData taskItemData;
    std::vector<wxString> taskDurations;
    try {
        taskDurations = taskItemData.GetHours(dateString);
    } catch (const sqlite::sqlite_exception& e) {
        pLogger->error("Error occured on TaskItemData::GetHours() - {0:d} : {1}", e.get_code(), e.what());
    }

    wxTimeSpan totalDuration;
    for (auto duration : taskDurations) {
        std::vector<std::string> durationSplit = util::lib::split(duration.ToStdString(), ':');

        wxTimeSpan currentDuration(std::atol(durationSplit[0].c_str()),
            std::atol(durationSplit[1].c_str()),
            (wxLongLong) std::atoll(durationSplit[2].c_str()));

        totalDuration += currentDuration;
    }

    pTotalHoursText->SetLabel(totalDuration.Format(constants::TotalHours));
}

void MainFrame::RefreshItems(wxDateTime date)
{
    wxString dateString = date.FormatISODate();

    data::TaskItemData taskItemData;
    std::vector<std::unique_ptr<model::TaskItemModel>> taskItems;
    try {
        taskItems = taskItemData.GetByDate(dateString);
    } catch (const sqlite::sqlite_exception& e) {
        pLogger->error("Error occured on TaskItemData::GetByDate() - {0:d} : {1}", e.get_code(), e.what());
        return;
    }

    int listIndex = 0;
    int columnIndex = 0;
    for (const auto& taskItem : taskItems) {
        listIndex = pListCtrl->InsertItem(columnIndex++, taskItem->GetProject()->GetDisplayName());
        pListCtrl->SetItem(listIndex, columnIndex++, taskItem->GetTask()->GetTaskDate().FormatISODate());
        pListCtrl->SetItem(listIndex,
            columnIndex++,
            taskItem->GetStartTime() ? wxString(taskItem->GetStartTime()->FormatISOTime()) : wxGetEmptyString());
        pListCtrl->SetItem(listIndex,
            columnIndex++,
            taskItem->GetEndTime() ? wxString(taskItem->GetEndTime()->FormatISOTime()) : wxGetEmptyString());
        pListCtrl->SetItem(listIndex, columnIndex++, taskItem->GetDuration());
        pListCtrl->SetItem(listIndex, columnIndex++, taskItem->GetCategory()->GetName());
        pListCtrl->SetItem(listIndex, columnIndex++, taskItem->GetDescription());

        pListCtrl->SetItemBackgroundColour(listIndex, taskItem->GetCategory()->GetColor());

        pListCtrl->SetItemPtrData(listIndex, (wxUIntPtr) taskItem->GetTaskItemId());

        columnIndex = 0;
    }
}

bool MainFrame::RunDatabaseBackup()
{
    if (pConfig->IsBackupEnabled()) {
        svc::DatabaseBackup dbBackup(pConfig, pLogger, pDatabase);
        bool result = dbBackup.Execute();
        return result;
    }

    return true;
}

void MainFrame::ShowInfoBarMessageForAdd(int modalRetCode, const wxString& item)
{
    if (modalRetCode == wxID_OK) {
        pInfoBar->ShowMessage(constants::OnSuccessfulAdd(item), wxICON_INFORMATION);
    } else if (modalRetCode == ids::ID_ERROR_OCCURED) {
        pInfoBar->ShowMessage(constants::OnErrorAdd(item), wxICON_ERROR);
    }

    pDismissInfoBarTimer->Start(1500);
}

void MainFrame::ShowInfoBarMessageForEdit(int modalRetCode, const wxString& item)
{
    if (modalRetCode == wxID_OK) {
        pInfoBar->ShowMessage(constants::OnSuccessfulEdit(item), wxICON_INFORMATION);
    } else if (modalRetCode == ids::ID_ERROR_OCCURED) {
        pInfoBar->ShowMessage(constants::OnErrorEdit(item), wxICON_ERROR);
    }

    pDismissInfoBarTimer->Start(1500);
}

void MainFrame::ShowInfoBarMessageForDelete(bool success)
{
    if (success) {
        pInfoBar->ShowMessage(wxT("Successfully deleted"), wxICON_INFORMATION);
    } else {
        pInfoBar->ShowMessage(wxT("Error deleting task"), wxICON_ERROR);
    }

    pDismissInfoBarTimer->Start(1500);
}
} // namespace app::frm
