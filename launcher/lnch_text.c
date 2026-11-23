/* The launcher's texts in the game's five languages (UTF-8). A row: the key,
 * then English, French, German, Italian, Spanish. %s / %d are filled by the
 * caller (launcher.c), in the same order in every language. */
#include <string.h>
#include "lnch.h"

static const char* const T[][6] = {
    {"news", "NEWS", "NOUVEAUTÉS", "NEUIGKEITEN", "NOVITÀ", "NOVEDADES"},
    {"nav_all", "see all →", "tout voir →", "alle ansehen →", "vedi tutto →", "ver todo →"},
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
     "Etwas ist schiefgelaufen: der Grund steht oben", "Qualcosa è andato storto: il motivo è qui sopra", "Algo salió mal: el motivo está arriba"},
    {"st_no_updates_here", "Ready to play (updates and the HD pack: Windows only for now)",
     "Prêt à jouer (mises à jour et pack HD : Windows seulement pour l'instant)",
     "Bereit zum Spielen (Updates und HD-Paket: vorerst nur Windows)",
     "Pronto per giocare (aggiornamenti e pacchetto HD: per ora solo Windows)",
     "Listo para jugar (actualizaciones y paquete HD: por ahora solo Windows)"},
    {"retry", "RETRY", "RÉESSAYER", "ERNEUT VERSUCHEN", "RIPROVA", "REINTENTAR"},
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
     "Un unico schermo largo, 3D nitido, pacchetto HD, menu moderni",
     "Una sola pantalla ancha, 3D nítido, paquete HD, menús modernos"},
    {"mode_note", "The mode sets the whole look of the game at once", "Le mode règle tout l'aspect du jeu d'un coup",
     "Der Modus stellt das ganze Aussehen des Spiels auf einmal ein", "La modalità imposta tutto l'aspetto del gioco in un colpo",
     "El modo ajusta todo el aspecto del juego de una vez"},
    {"window", "Window", "Fenêtre", "Fenster", "Finestra", "Ventana"},
    {"fullscreen", "Full screen", "Plein écran", "Vollbild", "Schermo intero", "Pantalla completa"},
    {"windowed", "Window", "Fenêtre", "Fenster", "Finestra", "Ventana"},
    {"volume", "Volume", "Volume", "Lautstärke", "Volume", "Volumen"},
    {"keys_pad", "Controller buttons", "Touches de la manette", "Controller-Tasten", "Tasti del controller", "Botones del mando"},
    {"keys_keyboard", "Keyboard keys", "Touches du clavier", "Tastatur", "Tasti della tastiera", "Teclas del teclado"},
    {"configure", "Set up… →", "Configurer… →", "Einrichten… →", "Configura… →", "Configurar… →"},
    {"reset_defaults", "Reset to defaults", "Rétablir par défaut", "Standard wiederherstellen", "Ripristina predefiniti", "Restablecer valores"},
    {"lang_name", "English", "Français", "Deutsch", "Italiano", "Español"},
    {"dl_waiting", "Waiting", "En attente", "Wartet", "In attesa", "En espera"},
    {"dl_update", "Update %s", "Mise à jour %s", "Update %s", "Aggiornamento %s", "Actualización %s"},
    {"dl_pack", "HD pack (%d/%d)", "Pack HD (%d/%d)", "HD-Paket (%d/%d)", "Pacchetto HD (%d/%d)", "Paquete HD (%d/%d)"},
    {"dl_done", "Everything is downloaded", "Tout est téléchargé", "Alles ist heruntergeladen", "Tutto è scaricato", "Todo está descargado"},
    {"in_copy", "Putting the new version in place", "Mise en place de la nouvelle version", "Neue Version wird eingerichtet",
     "Installazione della nuova versione", "Colocando la nueva versión"},
    {"in_build", "Building: %s", "Compilation : %s", "Erstellen: %s", "Compilazione: %s", "Compilando: %s"},
    {"in_pack", "HD pack: %d/%d", "Pack HD : %d/%d", "HD-Paket: %d/%d", "Pacchetto HD: %d/%d", "Paquete HD: %d/%d"},
    {"in_done", "Everything is installed", "Tout est installé", "Alles ist installiert", "Tutto è installato", "Todo está instalado"},
    {"offline_news", "Offline: the notes of this version", "Hors ligne : les notes de cette version",
     "Offline: die Notizen dieser Version", "Offline: le note di questa versione", "Sin conexión: las notas de esta versión"},
    {"month", "JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC", "JAN FÉV MAR AVR MAI JUN JUL AOÛ SEP OCT NOV DÉC",
     "JAN FEB MÄR APR MAI JUN JUL AUG SEP OKT NOV DEZ", "GEN FEB MAR APR MAG GIU LUG AGO SET OTT NOV DIC",
     "ENE FEB MAR ABR MAY JUN JUL AGO SEP OCT NOV DIC"},
};
#define NT (int)(sizeof T / sizeof T[0])

const char* tr_key(int i) { return i >= 0 && i < NT ? T[i][0] : NULL; }

static int g_lang = 1;
void tr_set_language(int lang) { g_lang = lang >= 1 && lang <= 5 ? lang : 1; }

const char* tr(const char* key) {
    for (int i = 0; i < NT; i++)
        if (!strcmp(T[i][0], key)) return T[i][g_lang] && T[i][g_lang][0] ? T[i][g_lang] : T[i][1];
    return key;
}
