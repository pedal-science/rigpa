#include "MakeHeuristics.h"

#include <QList>
#include <QRegularExpression>

#include <utility>

namespace rigpa::make {

namespace {

// One canonical brand and the spellings seen in capture file names (ljoss's table, verbatim).
// Speakers and microphones are under their own makers: that is what a cab IR is named after.
const QList<std::pair<QString, QStringList>> &brands()
{
    static const QList<std::pair<QString, QStringList>> table = {
        {"Fender", {"fender", "fndr"}}, {"Marshall", {"marshall", "mrsh"}}, {"Vox", {"vox"}},
        {"Mesa/Boogie", {"mesa boogie", "mesa", "boogie"}}, {"Orange", {"orange", "orng"}},
        {"Ampeg", {"ampeg", "ampg"}}, {"Peavey", {"peavey"}}, {"Hiwatt", {"hiwatt"}},
        {"Friedman", {"friedman"}}, {"Soldano", {"soldano"}}, {"Bogner", {"bogner"}},
        {"Diezel", {"diezel"}}, {"EVH", {"evh"}}, {"PRS", {"prs", "paul reed smith"}},
        {"Matchless", {"matchless"}}, {"Dumble", {"dumble"}}, {"Supro", {"supro"}},
        {"Silvertone", {"silvertone"}}, {"Gibson", {"gibson"}}, {"Gretsch", {"gretsch"}},
        {"Rickenbacker", {"rickenbacker"}}, {"Music Man", {"music man", "musicman"}},
        {"Roland", {"roland"}}, {"Boss", {"boss"}}, {"Line 6", {"line 6", "line6"}},
        {"Kemper", {"kemper"}}, {"Neural DSP", {"neural dsp", "neural"}}, {"ToneX", {"tonex", "tone x"}},
        {"Yamaha", {"yamaha"}}, {"Darkglass", {"darkglass"}},
        {"Gallien-Krueger", {"gallien krueger", "gallien"}}, {"Markbass", {"markbass", "mark bass"}},
        {"Aguilar", {"aguilar"}}, {"Tech 21", {"tech 21", "tech21", "sansamp", "sans amp"}},
        {"Zvex", {"zvex"}}, {"Fulltone", {"fulltone"}}, {"Ibanez", {"ibanez"}},
        {"Celestion", {"celestion", "v30", "greenback", "creamback", "g12m", "g12h", "alnico"}},
        {"Jensen", {"jensen"}}, {"Eminence", {"eminence"}}, {"Scumback", {"scumback"}},
        {"Shure", {"shure", "sm57", "sm58", "sm7"}}, {"Sennheiser", {"sennheiser", "md421", "e609", "e906"}},
        {"Royer", {"royer", "r121"}}, {"Audix", {"audix"}}, {"Neumann", {"neumann", "u87", "u47"}},
        {"AKG", {"akg"}}, {"Beyerdynamic", {"beyerdynamic", "beyer dynamic", "m160"}},
        {"Electro-Voice", {"electro voice", "electrovoice", "re20"}}, {"Heil", {"heil", "pr30"}},
        {"Rode", {"rode"}}, {"Warm Audio", {"warm audio"}}, {"Universal Audio", {"universal audio"}},
        {"Fractal", {"fractal"}}, {"HeadRush", {"headrush"}},
    };
    return table;
}

// Lowercased alphanumeric words: punctuation, dashes and underscores all separate.
QStringList words(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral("[^\\p{L}\\p{N}]+"));
    return text.toLower().split(separators, Qt::SkipEmptyParts);
}

} // namespace

QString canonical(const QString &text)
{
    // Bracketed text in a file or folder name lists what the file is *for* ("48k (Fractal,
    // Line 6, Suhr, etc.)"), not who made it, so it never names the brand.
    static const QRegularExpression brackets(QStringLiteral("\\([^)]*\\)|\\[[^\\]]*\\]"));
    QString plain = text;
    plain.remove(brackets);
    const QStringList w = words(plain);
    if (w.isEmpty())
        return {};
    QString best;
    qsizetype bestLength = 0;
    for (const auto &[brand, spellings] : brands()) {
        for (const QString &spelling : spellings) {
            const QStringList phrase = words(spelling);
            if (phrase.isEmpty() || phrase.size() > w.size())
                continue;
            for (qsizetype start = 0; start + phrase.size() <= w.size(); ++start) {
                if (w.mid(start, phrase.size()) == phrase && phrase.size() > bestLength) {
                    best = brand;
                    bestLength = phrase.size(); // longest phrase wins: "mesa boogie" over "mesa"
                }
            }
        }
    }
    return best;
}

QString guess(const QString &claimed, const QString &fileStem, const QStringList &directories)
{
    const QString c = claimed.trimmed();
    if (!c.isEmpty()) {
        const QString brand = canonical(c);
        return brand.isEmpty() ? c : brand;
    }
    // Deepest folder first: a brand folder is a stronger signal than any word of a file name.
    for (auto it = directories.crbegin(); it != directories.crend(); ++it) {
        const QString brand = canonical(*it);
        if (!brand.isEmpty())
            return brand;
    }
    const QString brand = canonical(fileStem);
    return brand.isEmpty() ? kUnknown : brand;
}

} // namespace rigpa::make
