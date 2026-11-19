# Met à jour l'en-tête de version de chaque fichier du firmware (version du projet, date de
# dernière modification du fichier d'après git, auteur). Lancer après chaque série de
# modifications :  python3 outils/entetes.py <version>   (ex. 0.9.1)
import os, re, subprocess, sys, datetime
ICI = os.path.dirname(os.path.abspath(__file__))
FW = os.path.join(ICI, "..", "firmware", "CaravaneDemo")
AUTEUR = "Alexandre Phulpin (développement assisté par Claude)"
DEPOT = "https://github.com/alphulpin/Caravane"
DEBUT, FIN = "// ==== Caravane ", "// ====\n"

version = sys.argv[1] if len(sys.argv) > 1 else None
cfg = open(os.path.join(FW, "Config.h"), encoding="utf-8").read()
if version:
    cfg = re.sub(r'#define FIRMWARE_VERSION "[^"]*"', f'#define FIRMWARE_VERSION "{version}"', cfg)
    open(os.path.join(FW, "Config.h"), "w", encoding="utf-8").write(cfg)
else:
    version = re.search(r'#define FIRMWARE_VERSION "([^"]*)"', cfg).group(1)

def sans_entete(texte):
    if texte.startswith(DEBUT):
        return texte[texte.index(FIN) + len(FIN):]
    return texte

for nom in sorted(os.listdir(FW)):
    if not nom.endswith((".ino", ".h", ".cpp")):
        continue
    chemin = os.path.join(FW, nom)
    ancien = open(chemin, encoding="utf-8").read()
    corps = sans_entete(ancien)
    # date : aujourd'hui si le code (hors en-tête) a changé depuis le dernier commit ; sinon celle
    # de l'en-tête du dernier commit, à défaut la date de ce commit
    rel = os.path.relpath(chemin, os.path.join(ICI, ".."))
    r = subprocess.run(["git", "show", f"HEAD:{rel}"], cwd=os.path.join(ICI, ".."), capture_output=True, text=True)
    commite = r.stdout if r.returncode == 0 else None
    if commite is None or sans_entete(commite) != corps:
        date = datetime.date.today().isoformat()
    else:
        m = re.search(r"dernière modification : (\S+)", commite)
        date = m.group(1) if m else subprocess.run(
            ["git", "log", "-1", "--format=%cs", "--", rel], cwd=os.path.join(ICI, ".."),
            capture_output=True, text=True).stdout.strip()
    entete = (f"{DEBUT}{'=' * (88 - len(DEBUT))}\n"
              f"// {nom} — firmware CaravaneDemo, version {version} — dernière modification : {date}\n"
              f"// Auteur : {AUTEUR}\n"
              f"// Dépôt : {DEPOT}\n"
              f"{FIN}")
    if entete + corps != ancien:
        open(chemin, "w", encoding="utf-8").write(entete + corps)
        print("en-tête :", nom, date)
