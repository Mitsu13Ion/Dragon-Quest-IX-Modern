/* The launcher's texts in the game's five languages (UTF-8). A row: the key,
 * then English, French, German, Italian, Spanish. %s / %d are filled by the
 * caller (launcher.c), in the same order in every language. */
#include <string.h>
#include "lnch.h"

static const char* const T[][6] = {
    {"news", "NEWS", "NOUVEAUTÉS", "NEUIGKEITEN", "NOVITÀ", "NOVEDADES"},
    {"nav_all", "see all ›", "tout voir ›", "alle ansehen ›", "vedi tutto ›", "ver todo ›"},
    {"verify", "VERIFY", "VÉRIFIER", "PRÜFEN", "VERIFICA", "VERIFICAR"},
    {"options", "OPTIONS", "OPTIONS", "OPTIONEN", "OPZIONI", "OPCIONES"},
    {"play", "PLAY", "JOUER", "SPIELEN", "GIOCA", "JUGAR"},
    {"download", "DOWNLOAD", "TÉLÉCHARGEMENT", "DOWNLOAD", "DOWNLOAD", "DESCARGA"},
    {"install", "INSTALL", "INSTALLATION", "INSTALLATION", "INSTALLAZIONE", "INSTALACIÓN"},
    {"st_ready", "Everything is up to date, ready to play!", "Tout est à jour, prêt à jouer !",
     "Alles ist aktuell, bereit zum Spielen!", "Tutto è aggiornato, pronto per giocare!", "¡Todo está al día, listo para jugar!"},
    {"st_offline", "Offline: updates not checked", "Hors ligne : mises à jour non vérifiées",
     "Offline: Updates nicht geprüft", "Offline: aggiornamenti non verificati", "Sin conexión: actualizaciones no comprobadas"},
    {"st_updating", "Updating the game…", "Mise à jour du jeu…", "Spiel wird aktualisiert…", "Aggiornamento del gioco…", "Actualizando el juego…"},
    {"st_building", "Building the game from your ROM…", "Compilation du jeu depuis votre ROM…",
     "Das Spiel wird aus deinem ROM erstellt…", "Compilazione del gioco dalla tua ROM…", "Compilando el juego desde tu ROM…"},
    {"st_pack", "Getting the HD pack…", "Préparation du pack HD…", "HD-Paket wird vorbereitet…", "Preparazione del pacchetto HD…", "Preparando el paquete HD…"},
    {"st_failed", "Something went wrong: the reason is above", "Un problème est survenu : la raison est au-dessus",
     "Etwas ist schiefgelaufen: Der Grund steht oben", "Qualcosa è andato storto: il motivo è qui sopra", "Algo salió mal: el motivo está arriba"},
    {"st_no_updates_here", "Ready to play (updates and the HD pack: Windows only for now)",
     "Prêt à jouer (mises à jour et pack HD : Windows seulement pour l'instant)",
     "Bereit zum Spielen (Updates und HD-Paket: vorerst nur Windows)",
     "Pronto per giocare (aggiornamenti e pacchetto HD: per ora solo Windows)",
     "Listo para jugar (actualizaciones y paquete HD: por ahora solo Windows)"},
    {"retry", "RETRY", "RÉESSAYER", "WIEDERHOLEN", "RIPROVA", "REINTENTAR"},
    {"options_title", "GAME OPTIONS", "OPTIONS DU JEU", "SPIELOPTIONEN", "OPZIONI DI GIOCO", "OPCIONES DEL JUEGO"},
    {"options_note", "used when the game starts", "prises en compte au lancement", "gelten beim Spielstart",
     "applicate all'avvio del gioco", "se aplican al iniciar el juego"},
    {"mode_classic", "Classic", "Classique", "Klassisch", "Classica", "Clásico"},
    {"mode_classic_help", "Like back then on the DS: two screens, the original picture",
     "Comme à l'époque sur DS : deux écrans, image d'origine",
     "Wie damals auf dem DS: zwei Bildschirme, Originalbild",
     "Come ai tempi del DS: due schermi, immagine originale",
     "Como antes en la DS: dos pantallas, imagen original"},
    {"mode_modern", "Modern", "Moderne", "Modern", "Moderna", "Moderno"},
    {"mode_modern_help", "One wide screen, sharp 3D, HD pack, modern menus",
     "Un seul écran large, 3D nette, pack HD, menus modernes",
     "Ein breiter Bildschirm, scharfes 3D, HD-Paket, moderne Menüs",
     "Un unico schermo panoramico, 3D nitido, pacchetto HD, menu moderni",
     "Una sola pantalla panorámica, 3D nítido, paquete HD, menús modernos"},
    {"mode_note", "The mode sets the whole look of the game at once", "Le mode règle tout l'aspect du jeu d'un coup",
     "Der Modus stellt das ganze Aussehen des Spiels auf einmal ein", "La modalità imposta tutto l'aspetto del gioco in un colpo solo",
     "El modo ajusta todo el aspecto del juego de una vez"},
    {"window", "Display", "Affichage", "Anzeige", "Schermo", "Pantalla"},
    {"fullscreen", "Full screen", "Plein écran", "Vollbild", "Schermo intero", "Pantalla completa"},
    {"windowed", "Window", "Fenêtre", "Fenster", "Finestra", "Ventana"},
    {"volume", "Volume", "Volume", "Lautstärke", "Volume", "Volumen"},
    {"keys_pad", "Controller buttons", "Touches de la manette", "Controller-Tasten", "Tasti del controller", "Botones del mando"},
    {"keys_keyboard", "Keyboard keys", "Touches du clavier", "Tastatur", "Tasti della tastiera", "Teclas del teclado"},
    {"configure", "Set up… ›", "Configurer… ›", "Einrichten… ›", "Configura… ›", "Configurar… ›"},
    {"reset_defaults", "Reset to defaults", "Rétablir par défaut", "Standard wiederherstellen", "Ripristina predefiniti", "Restablecer valores"},
    {"lang_name", "English", "Français", "Deutsch", "Italiano", "Español"},
    {"dl_waiting", "Waiting", "En attente", "Wartet", "In attesa", "En espera"},
    {"dl_update", "Update %s", "Mise à jour %s", "Update %s", "Aggiornamento %s", "Actualización %s"},
    {"dl_pack", "HD pack (%d/%d)", "Pack HD (%d/%d)", "HD-Paket (%d/%d)", "Pacchetto HD (%d/%d)", "Paquete HD (%d/%d)"},
    {"dl_done", "Everything is downloaded", "Tout est téléchargé", "Alles ist heruntergeladen", "Tutto è scaricato", "Todo está descargado"},
    {"in_copy", "Putting the new version in place", "Mise en place de la nouvelle version", "Neue Version wird eingerichtet",
     "Installazione della nuova versione", "Instalando la nueva versión"},
    {"in_build", "%s", "%s", "%s", "%s", "%s"},   /* the build's step title alone (translated below) */
    {"in_pack", "HD pack: %d/%d", "Pack HD : %d/%d", "HD-Paket: %d/%d", "Pacchetto HD: %d/%d", "Paquete HD: %d/%d"},
    {"in_done", "Everything is installed", "Tout est installé", "Alles ist installiert", "Tutto è installato", "Todo está instalado"},
    {"offline_news", "Offline: the notes of this version", "Hors ligne : les notes de cette version",
     "Offline: die Hinweise zu dieser Version", "Offline: le note di questa versione", "Sin conexión: las notas de esta versión"},
    {"month", "JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC", "JAN FÉV MAR AVR MAI JUN JUL AOÛ SEP OCT NOV DÉC",
     "JAN FEB MÄR APR MAI JUN JUL AUG SEP OKT NOV DEZ", "GEN FEB MAR APR MAG GIU LUG AGO SET OTT NOV DIC",
     "ENE FEB MAR ABR MAY JUN JUL AGO SEP OCT NOV DIC"},
    {"update_available", "Version %s is out (updates are off in the settings)", "La version %s est sortie (mises à jour désactivées dans les réglages)",
     "Version %s ist da (Updates sind in den Einstellungen aus)", "La versione %s è uscita (aggiornamenti disattivati nelle impostazioni)",
     "La versión %s ya salió (actualizaciones desactivadas en los ajustes)"},
    {"st_rom", "Choose your Dragon Quest IX ROM (European or American, .nds or .zip)", "Choisissez votre ROM de Dragon Quest IX (européenne ou américaine, .nds ou .zip)",
     "Wähle dein Dragon Quest IX-ROM (europäisch oder amerikanisch, .nds oder .zip)", "Scegli la tua ROM di Dragon Quest IX (europea o americana, .nds o .zip)",
     "Elige tu ROM de Dragon Quest IX (europea o americana, .nds o .zip)"},
    {"choose_rom", "CHOOSE THE ROM", "CHOISIR LA ROM", "ROM WÄHLEN", "SCEGLI LA ROM", "ELEGIR LA ROM"},
    {"rom_title", "Your Dragon Quest IX ROM", "Votre ROM de Dragon Quest IX", "Dein Dragon Quest IX-ROM", "La tua ROM di Dragon Quest IX", "Tu ROM de Dragon Quest IX"},
    {"rom_copy", "Copying your ROM…", "Copie de votre ROM…", "Dein ROM wird kopiert…", "Copia della tua ROM…", "Copiando tu ROM…"},
    {"rom_bad", "That ROM can't be used (.nds or .zip, readable)", "Cette ROM ne peut pas servir (.nds ou .zip, lisible)",
     "Dieses ROM ist nicht verwendbar (.nds oder .zip, lesbar)", "Questa ROM non si può usare (.nds o .zip, leggibile)", "Esa ROM no sirve (.nds o .zip, legible)"},
    {"err_build", "The build stopped: build/build.log says why", "La compilation s'est arrêtée : build/build.log dit pourquoi",
     "Das Erstellen wurde abgebrochen: build/build.log nennt den Grund", "La compilazione si è fermata: build/build.log dice perché",
     "La compilación se detuvo: build/build.log dice por qué"},
    {"err_download", "Download failed: %s", "Téléchargement impossible : %s", "Download fehlgeschlagen: %s", "Download non riuscito: %s", "Descarga fallida: %s"},
    {"err_github", "GitHub didn't answer (are you online?)", "GitHub ne répond pas (êtes-vous en ligne ?)", "GitHub antwortet nicht (bist du online?)",
     "GitHub non risponde (sei online?)", "GitHub no responde (¿estás conectado?)"},
    /* the build's step titles (tools/build-game.sh step, BUILD.bat progress): the key is the English text */
    {"Checking the ROM", "Checking the ROM", "Vérification de la ROM", "ROM wird geprüft", "Verifica della ROM", "Comprobando la ROM"},
    {"Checking your ROM", "Checking your ROM", "Vérification de votre ROM", "Dein ROM wird geprüft", "Verifica della tua ROM", "Comprobando tu ROM"},
    {"Extracting the game's code", "Extracting the game's code", "Extraction du code du jeu", "Spielcode wird entpackt", "Estrazione del codice del gioco", "Extrayendo el código del juego"},
    {"Recompiling it to C (about a minute)", "Recompiling it to C (about a minute)", "Recompilation en C (environ une minute)",
     "Umwandlung in C (etwa eine Minute)", "Ricompilazione in C (circa un minuto)", "Recompilando a C (alrededor de un minuto)"},
    {"Compiling (the first time takes a while; later builds only redo what changed)", "Compiling (the first time takes a while; later builds only redo what changed)",
     "Compilation (longue la première fois, ensuite seul ce qui a changé est refait)",
     "Kompilieren (beim ersten Mal dauert es, danach nur das Geänderte)",
     "Compilazione (la prima volta è lunga, poi solo ciò che è cambiato)",
     "Compilando (la primera vez tarda, después solo lo que cambió)"},
    {"Putting the game folder together", "Putting the game folder together", "Assemblage du dossier du jeu", "Spielordner wird zusammengestellt",
     "Preparazione della cartella del gioco", "Preparando la carpeta del juego"},
    {"Installing the build tools (MSYS2, one time)", "Installing the build tools (MSYS2, one time)", "Installation des outils de compilation (MSYS2, une seule fois)",
     "Build-Werkzeuge werden installiert (MSYS2, einmalig)", "Installazione degli strumenti di compilazione (MSYS2, una volta sola)",
     "Instalando las herramientas de compilación (MSYS2, una sola vez)"},
    {"Checking the build tools", "Checking the build tools", "Vérification des outils de compilation", "Build-Werkzeuge werden geprüft",
     "Verifica degli strumenti di compilazione", "Comprobando las herramientas de compilación"},
    {"Checking the build tools (the first time may download some)", "Checking the build tools (the first time may download some)",
     "Vérification des outils de compilation (la première fois, certains sont téléchargés)",
     "Build-Werkzeuge werden geprüft (beim ersten Mal wird einiges geladen)",
     "Verifica degli strumenti di compilazione (la prima volta ne scarica alcuni)",
     "Comprobando las herramientas de compilación (la primera vez se descargan algunas)"},
    {"Done", "Done", "Terminé", "Fertig", "Fatto", "Hecho"},
};
#define NT (int)(sizeof T / sizeof T[0])

const char* tr_key(int i) { return i >= 0 && i < NT ? T[i][0] : NULL; }

static int g_lang = 1;
void tr_set_language(int lang) { g_lang = lang >= 1 && lang <= 5 ? lang : 1; }

const char* tr_in(const char* key, int lang) {
    if (lang < 1 || lang > 5) lang = 1;
    for (int i = 0; i < NT; i++)
        if (!strcmp(T[i][0], key)) return T[i][lang] && T[i][lang][0] ? T[i][lang] : T[i][1];
    return key;
}
const char* tr(const char* key) {
    for (int i = 0; i < NT; i++)
        if (!strcmp(T[i][0], key)) return T[i][g_lang] && T[i][g_lang][0] ? T[i][g_lang] : T[i][1];
    return key;
}

int tr_has(int i, int lang) { return i >= 0 && i < NT && lang >= 1 && lang <= 5 && T[i][lang] && T[i][lang][0]; }
