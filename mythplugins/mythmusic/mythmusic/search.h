#ifndef SEARCH_H_
#define SEARCH_H_

#include <qregexp.h>
//Added by qt3to4:
#include <QLabel>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythwidgets.h>

class SearchListBoxItem: public Q3ListBoxText
{
  public:
    SearchListBoxItem(const QString &text, unsigned int id)
       : Q3ListBoxText(text), id(id) {};

    unsigned int getId() { return id; }

  private:
    virtual void paint(QPainter *p);
    unsigned int id;
};

class SearchDialog: public MythPopupBox
{
  Q_OBJECT

  public:

    SearchDialog(MythMainWindow *parent, const char *name = 0); 
    ~SearchDialog();

    void getWhereClause(QString &whereClause);

  protected slots:

    void searchTextChanged(const QString &searchText);
    void itemSelected(int i);

  private:

    void runQuery(QString searchText);

    QLabel              *caption;
    MythListBox         *listbox;  
    MythLineEdit        *searchText;
    QAbstractButton             *cancelButton;
    QAbstractButton             *okButton;

    QString              whereClause;
};

#endif
