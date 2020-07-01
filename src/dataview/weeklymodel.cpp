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

#include "weeklymodel.h"

#include <algorithm>
#include <iterator>

#include "../data/taskitemdata.h"

namespace app::dv
{
WeeklyTreeModelNode::WeeklyTreeModelNode(WeeklyTreeModelNode* parent,
    const wxString& projectName,
    const wxString& duration,
    const wxString& categoryName,
    const wxString& description,
    int taskItemId)
    : pParent(parent)
    , mProjectName(projectName)
    , mDuration(duration)
    , mCategoryName(categoryName)
    , mDescription(description)
    , mTaskItemId(taskItemId)
    , bContainer(false)
{
}

WeeklyTreeModelNode::WeeklyTreeModelNode(WeeklyTreeModelNode* parent, const wxString& branch)
    : pParent(parent)
    , mProjectName(branch)
    , bContainer(true)
{
}

WeeklyTreeModelNode::~WeeklyTreeModelNode()
{
    std::size_t count = mChildren.GetCount();
    for (std::size_t i = 0; i < count; i++) {
        WeeklyTreeModelNode* child = mChildren[i];
        delete child;
    }
}

bool WeeklyTreeModelNode::IsContainer() const
{
    return bContainer;
}

WeeklyTreeModelNode* WeeklyTreeModelNode::GetParent()
{
    return pParent;
}

WeeklyTreeModelNodePtrArray& WeeklyTreeModelNode::GetChildren()
{
    return mChildren;
}

WeeklyTreeModelNode* WeeklyTreeModelNode::GetNthChild(unsigned int n)
{
    return mChildren.Item(n);
}

void WeeklyTreeModelNode::Insert(WeeklyTreeModelNode* child, unsigned int n)
{
    mChildren.Insert(child, n);
}

void WeeklyTreeModelNode::Append(WeeklyTreeModelNode* child)
{
    mChildren.Add(child);
}

const unsigned int WeeklyTreeModelNode::GetChildCount() const
{
    return mChildren.Count();
}

wxString WeeklyTreeModelNode::GetProjectName() const
{
    return mProjectName;
}

wxString WeeklyTreeModelNode::GetDuration() const
{
    return mDuration;
}

wxString WeeklyTreeModelNode::GetCategoryName() const
{
    return mCategoryName;
}

wxString WeeklyTreeModelNode::GetDescription() const
{
    return mDescription;
}

int WeeklyTreeModelNode::GetTaskItemId() const
{
    return mTaskItemId;
}

void WeeklyTreeModelNode::SetProjectName(const wxString& value)
{
    mProjectName = value;
}

void WeeklyTreeModelNode::SetDuration(const wxString& value)
{
    mDuration = value;
}

void WeeklyTreeModelNode::SetCategoryName(const wxString& value)
{
    mCategoryName = value;
}

void WeeklyTreeModelNode::SetDescription(const wxString& value)
{
    mDescription = value;
}

void WeeklyTreeModelNode::SetTaskItemId(int taskItemId)
{
    mTaskItemId = taskItemId;
}

// WeeklyTreeModel
WeeklyTreeModel::WeeklyTreeModel(const DateTraverser& dateTraverser)
    : mWeeklyTasksMap()
    , mDateTraverser(dateTraverser)
{
    SetupNodes();

    SetupMondayNode();
    SetupTuesdayNode();
    SetupWednesdayNode();
    SetupThursdayNode();
    SetupFridayNode();
    SetupSaturdayNode();
    SetupSundayNode();
}

WeeklyTreeModel::~WeeklyTreeModel()
{
    delete pRoot;
}

// This method should only be used once from WeeklyTaskViewDialog::FillControls
void WeeklyTreeModel::InitBatch(std::vector<std::unique_ptr<model::TaskItemModel>>& taskItems)
{
    for (auto& taskItem : taskItems) {
        wxString taskDateIndex = taskItem->GetTask()->GetTaskDate();
        mWeeklyTasksMap[taskDateIndex].push_back(std::move(taskItem));
    }

    auto& mondayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Monday)];
    AddMany(pMonday, mondayTaskItems);

    auto& tuesdayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Tuesday)];
    AddMany(pTuesday, tuesdayTaskItems);

    auto& wednesdayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Wednesday)];
    AddMany(pWednesday, wednesdayTaskItems);

    auto& thursdayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Thursday)];
    AddMany(pThursday, thursdayTaskItems);

    auto& fridayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Friday)];
    AddMany(pFriday, fridayTaskItems);

    auto& saturdayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Saturday)];
    AddMany(pSaturday, saturdayTaskItems);

    auto& sundayTaskItems = mWeeklyTasksMap[mDateTraverser.GetDayISODate(DateTraverser::Days::Sunday)];
    AddMany(pSunday, sundayTaskItems);
}

unsigned int WeeklyTreeModel::GetColumnCount() const
{
    return Col_Max;
}

wxString WeeklyTreeModel::GetColumnType(unsigned int col) const
{
    if (col == Col_Id) {
        return "long";
    } else {
        return "string";
    }
}

void WeeklyTreeModel::GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const
{
    wxASSERT(item.IsOk());

    WeeklyTreeModelNode* node = (WeeklyTreeModelNode*) item.GetID();
    switch (col) {
    case Col_Project:
        variant = node->GetProjectName();
        break;
    case Col_Duration:
        variant = node->GetDuration();
        break;
    case Col_Category:
        variant = node->GetCategoryName();
        break;
    case Col_Description:
        variant = node->GetDescription();
        break;
    case Col_Id:
        variant = (long) node->GetTaskItemId();
        break;
    case Col_Max:
    default:
        wxLogError("WeeklyTreeModel::GetValue: wrong column %d", col);
        break;
    }
}

bool WeeklyTreeModel::SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col)
{
    wxASSERT(item.IsOk());

    WeeklyTreeModelNode* node = (WeeklyTreeModelNode*) item.GetID();
    switch (col) {
    case Col_Project:
        node->SetProjectName(variant.GetString());
        break;
    case Col_Duration:
        node->SetDuration(variant.GetString());
        break;
    case Col_Category:
        node->SetCategoryName(variant.GetString());
        break;
    case Col_Description:
        node->SetDescription(variant.GetString());
        break;
    case Col_Id:
        node->SetTaskItemId(variant.GetInteger());
        break;
    case Col_Max:
    default:
        wxLogError("WeeklyTreeModel::SetValue: wrong column %d", col);
        break;
    }

    return false;
}

bool WeeklyTreeModel::IsEnabled(const wxDataViewItem& item, unsigned int col) const
{
    return true;
}

wxDataViewItem WeeklyTreeModel::GetParent(const wxDataViewItem& item) const
{
    if (!item.IsOk()) {
        return wxDataViewItem(0);
    }

    WeeklyTreeModelNode* node = (WeeklyTreeModelNode*) item.GetID();

    if (node == pRoot) {
        return wxDataViewItem(0);
    }

    return wxDataViewItem((void*) node->GetParent());
}

bool WeeklyTreeModel::IsContainer(const wxDataViewItem& item) const
{
    if (!item.IsOk()) {
        return true;
    }

    WeeklyTreeModelNode* node = (WeeklyTreeModelNode*) item.GetID();
    return node->IsContainer();
}

unsigned int WeeklyTreeModel::GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const
{
    WeeklyTreeModelNode* node = (WeeklyTreeModelNode*) parent.GetID();
    if (!node) {
        array.Add(wxDataViewItem((void*) pRoot));
        return 1;
    }

    if (node->GetChildCount() == 0) {
        return 0;
    }

    unsigned int count = node->GetChildren().GetCount();
    for (unsigned int pos = 0; pos < count; pos++) {
        WeeklyTreeModelNode* child = node->GetChildren().Item(pos);
        array.Add(wxDataViewItem((void*) child));
    }

    return count;
}

wxDataViewItem WeeklyTreeModel::ExpandRootNode()
{
    return wxDataViewItem((void*) pRoot);
}

void WeeklyTreeModel::SetupNodes()
{
    wxString weekLabel = wxString::Format(wxT("Week %s - %s"),
        mDateTraverser.GetDayISODate(DateTraverser::Days::Monday),
        mDateTraverser.GetDayISODate(DateTraverser::Days::Sunday));

    pRoot = new WeeklyTreeModelNode(nullptr, weekLabel);
}

void WeeklyTreeModel::SetupMondayNode()
{
    wxString label = wxString::Format(wxT("Monday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Monday));

    pMonday = new WeeklyTreeModelNode(pRoot, label);

    pRoot->Append(pMonday);
}

void WeeklyTreeModel::SetupTuesdayNode()
{
    wxString label = wxString::Format(wxT("Tuesday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Tuesday));

    pTuesday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pTuesday);
}

void WeeklyTreeModel::SetupWednesdayNode()
{
    wxString label = wxString::Format(wxT("Wednesday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Wednesday));

    pWednesday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pWednesday);
}

void WeeklyTreeModel::SetupThursdayNode()
{
    wxString label = wxString::Format(wxT("Thursday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Thursday));

    pThursday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pThursday);
}

void WeeklyTreeModel::SetupFridayNode()
{
    wxString label = wxString::Format(wxT("Friday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Friday));

    pFriday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pFriday);
}

void WeeklyTreeModel::SetupSaturdayNode()
{
    wxString label = wxString::Format(wxT("Saturday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Saturday));

    pSaturday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pSaturday);
}

void WeeklyTreeModel::SetupSundayNode()
{
    wxString label = wxString::Format(wxT("Sunday %s"), mDateTraverser.GetDayISODate(DateTraverser::Days::Sunday));

    pSunday = new WeeklyTreeModelNode(pRoot, label);
    pRoot->Append(pSunday);
}

void WeeklyTreeModel::AddMany(WeeklyTreeModelNode* dayNodeToAdd,
    std::vector<std::unique_ptr<model::TaskItemModel>>& dayTasksToAdd)
{
    for (const auto& taskToAdd : dayTasksToAdd) {
        auto node = new WeeklyTreeModelNode(dayNodeToAdd,
            taskToAdd->GetProject()->GetDisplayName(),
            taskToAdd->GetDuration(),
            taskToAdd->GetCategory()->GetName(),
            taskToAdd->GetDescription(),
            taskToAdd->GetTaskItemId());

        dayNodeToAdd->Append(node);
    }
}

} // namespace app::dv
