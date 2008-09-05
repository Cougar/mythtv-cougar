#ifndef MYTHUI_BUTTONTREE_H_
#define MYTHUI_BUTTONTREE_H_

#include "mythuitype.h"
#include "mythgenerictree.h"
#include "mythuibuttonlist.h"

class MythUIButtonListItem;

/** \class MythUIButtonTree
 *
 * \brief A tree widget
 *
 */
class MythUIButtonTree : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButtonTree(MythUIType *parent, const QString &name);
   ~MythUIButtonTree();

    virtual bool keyPressEvent(QKeyEvent *);
    //virtual void gestureEvent(MythUIType *uitype, MythGestureEvent *event);

    bool AssignTree(MythGenericTree *tree);
    void Reset(void);
    bool SetNodeByString(QStringList route);
    bool SetNodeById(QList<int> route);
    bool SetCurrentNode(MythGenericTree *node);
    MythGenericTree* GetCurrentNode(void) const { return m_currentNode; }

    void SetActive(bool active);

    MythUIButtonListItem* GetItemCurrent(void) const;

  public slots:
    void handleSelect(MythUIButtonListItem* item);
    void handleClick(MythUIButtonListItem* item);

  signals:
    void itemSelected(MythUIButtonListItem* item);
    void itemClicked(MythUIButtonListItem* item);
    void nodeChanged(MythGenericTree* node);

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

  private:
    void Init(void);
    void SetTreeState();
    bool UpdateList(MythUIButtonList *list, MythGenericTree *node);

    void SwitchList(bool right);

    bool m_active;
    bool m_initialized;
    uint m_numLists;
    uint m_visibleLists;
    uint m_currentDepth;
    uint m_oldDepth;
    QList<MythUIButtonList*> m_buttonlists;
    MythUIButtonList *m_listTemplate;
    MythUIButtonList *m_activeList;
    uint m_activeListID;
    MythGenericTree *m_rootNode;
    MythGenericTree *m_currentNode;
    uint m_listSpacing;
};

#endif
