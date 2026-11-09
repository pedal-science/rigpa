// Moving rigs between banks: one place at a time, over the end of a bank into the next, a full bank
// giving up the rig it is displaced by, and an empty bank going.
#include <QtTest>

#include "rig/BanksModel.h"

using namespace rigpa;

class TestBanks : public QObject
{
    Q_OBJECT

    static QList<QStringList> full(const QString &prefix)
    {
        QStringList bank;
        for (int i = 0; i < BanksModel::kMaxRigs; ++i)
            bank.append(prefix + QString::number(i));
        return {bank};
    }

private slots:
    void withinABank()
    {
        int row = 0;
        const auto banks = BanksModel::step({{"A", "B", "C"}}, row, 1);
        QCOMPARE(banks, (QList<QStringList>{{"B", "A", "C"}}));
    }

    void overTheEndIntoTheNext()
    {
        int row = 1;
        QCOMPARE(BanksModel::step({{"A", "B"}, {"C"}}, row, 1), (QList<QStringList>{{"A"}, {"B", "C"}}));
        row = 2;
        QCOMPARE(BanksModel::step({{"A", "B"}, {"C"}}, row, -1), (QList<QStringList>{{"A", "B", "C"}}));
    }

    void pastTheLastBankIsANewOne()
    {
        int row = 1;
        QCOMPARE(BanksModel::step({{"A", "B"}}, row, 1), (QList<QStringList>{{"A"}, {"B"}}));
        row = 0;
        QCOMPARE(BanksModel::step({{"A"}}, row, 1), (QList<QStringList>{{"A"}})); // alone: nowhere further
        row = 0;
        QCOMPARE(BanksModel::step({{"A"}}, row, -1), (QList<QStringList>{{"A"}}));
    }

    void anEmptyBankGoes()
    {
        int row = 1;
        QCOMPARE(BanksModel::step({{"A"}, {"B"}}, row, -1), (QList<QStringList>{{"A", "B"}}));
    }

    void aFullBankGivesUpARig()
    {
        QList<QStringList> banks = {{"X"}};
        banks += full("r");
        int row = 0;
        const auto after = BanksModel::step(banks, row, 1);
        QCOMPARE(after.size(), 2);
        QCOMPARE(after[1].size(), BanksModel::kMaxRigs);
        QCOMPARE(after[1].first(), QStringLiteral("X"));
        QCOMPARE(after[0], QStringList({"r0"}));
    }
};

QTEST_MAIN(TestBanks)
#include "tst_banks.moc"
