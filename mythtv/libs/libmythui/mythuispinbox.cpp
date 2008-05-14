
#include "mythuispinbox.h"

MythUISpinBox::MythUISpinBox(MythUIType *parent, const char *name)
              : MythListButton(parent, name)
{
}

MythUISpinBox::~MythUISpinBox()
{
}

void MythUISpinBox::SetRange(int low, int high, int step)
{
    if ((high - low) == 0 || step == 0)
        return;

    Reset();

    int value = low;

    while (value <= high)
    {
        MythListButtonItem *item = new MythListButtonItem(this,
                                            QString::number(value));
        value = value + step;
    }

    SetPositionArrowStates();
}
