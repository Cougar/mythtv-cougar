#include "sr_items.h"
#include "sr_root.h"

RootSRGroup::RootSRGroup(ScheduledRecording& _rec,ManagedList* _parentList, QObject* _parent)
                  : ManagedListGroup( "rootGroup", NULL, _parentList, _parent, "rootGroup"),
                    schedRec(_rec)
{
    cancelItem = new DialogDoneListItem( QObject::tr("Cancel"), MythDialog::Rejected, NULL, parentList, this, "cancel");
    cancelItem->setState(MLS_BOLD);
    addItem(cancelItem);
    
    recordAsShownItem = new DialogDoneListItem( QObject::tr("Record this program as shown below"), MythDialog::Accepted, NULL,
                                                parentList, this, "recordAsShown");
    recordAsShownItem->setState(MLS_BOLD);
    addItem(recordAsShownItem);
    
    recType = new SRRecordingType(schedRec, _parentList, this);
    addItem(recType->getItem(), -1);
    connect(recType->getItem(), SIGNAL(changed(ManagedListItem*)), this, SLOT(itemChanged(ManagedListItem*)));
    
    profile = new SRProfileSelector(_rec, _parentList, this);
    addItem(profile->getItem(), -1);
    
    offsetGroup = new SROffsetGroup(_rec, _parentList, this);
    addItem(offsetGroup, -1);
    
    recGroup = new SRRecGroup(_rec, _parentList, this);
    addItem(recGroup->getItem(), -1);
    
    recPriority = new SRRecPriority(_rec, _parentList);
    addItem(recPriority->getItem(), -1);
    
    autoExpire = new SRAutoExpire(_rec, _parentList);
    addItem(autoExpire->getItem(), -1);
    
    dupSettings = new SRDupSettingsGroup(_rec, _parentList, this, this);
    addItem(dupSettings, -1);
    
    episodes = new SREpisodesGroup(_rec, _parentList, this, this);
    addItem(episodes, -1);
}
       
void RootSRGroup::setDialog(MythDialog* dlg) 
{
    cancelItem->setDialog(dlg); 
    recordAsShownItem->setDialog(dlg);
}

void RootSRGroup::itemChanged(ManagedListItem*) 
{ 
    bool multiEpisode = true;
    bool isScheduled = true;
    
    switch(recType->getValue().toInt())
    {
        case kNotRecording:
        case kDontRecord:
                isScheduled = false;
                break;

        case kSingleRecord:
        case kFindOneRecord:
        case kOverrideRecord:
                multiEpisode = false;
                break;
    }

    profile->getItem()->setEnabled(isScheduled);
    dupSettings->setEnabled(isScheduled & multiEpisode);
    offsetGroup->setEnabled(isScheduled);
    autoExpire->getItem()->setEnabled(isScheduled);
    recPriority->getItem()->setEnabled(isScheduled);
    episodes->setEnabled(isScheduled & multiEpisode);
    recGroup->getItem()->setEnabled(isScheduled);
}
