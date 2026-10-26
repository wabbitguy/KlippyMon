// =============================================================================
// Translation.h — KlippyMon localisation
// =============================================================================
//
// HOW TO USE
// ----------
// 1. Open Settings.h and uncomment ONE language only:
//
//      #define LANG_EN   // English  (default)
//      #define LANG_FR   // French
//      #define LANG_DE   // German
//      #define LANG_ES   // Spanish
//      #define LANG_NL   // Dutch
//      #define LANG_PT   // Portuguese
//      #define LANG_TR   // Turkish
//
// 2. Recompile and flash. That's it!
//
// FONT NOTE
// ---------
// strNozzle, strProgress, strBed, strIdle, strPrep and strWifiReset are drawn
// with the built-in Font 2, which has plain ASCII only: write them without
// accents (e.g. "Duese", "Progres"), or the accented letters are left out.
// The other screen texts use NotoSansBold15 (data folder), which has Latin-1
// plus Turkish ğ ı İ ş Ş. The web page text can use any character.
//
// GAUGE HEADING NOTE
// ------------------
// strNozzle, strProgress, strBed must be short — limited pixel width on
// the 240px wide screen. strPrep must be 5 chars or fewer — confined space
// inside the gauge arc.
//
// FPT (Finish Print Time) and ETA are universal — no translation needed.
// KlippyMon stays as-is on the network regardless of language.
// =============================================================================

#ifndef TRANSLATION_H
#define TRANSLATION_H

// -----------------------------------------------------------------------------
// Default to English if nothing is defined
// -----------------------------------------------------------------------------
#if !defined(LANG_EN) && !defined(LANG_FR) && !defined(LANG_DE) && \
    !defined(LANG_ES) && !defined(LANG_NL) && !defined(LANG_PT) && \
    !defined(LANG_TR)
  #define LANG_EN
#endif

// =============================================================================
//  ENGLISH
// =============================================================================
#ifdef LANG_EN

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Nozzle";
const char strProgress[]    = "Progress";
const char strBed[]         = "Bed";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Idle";
const char strPrep[]        = "PREP";


// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Total";
const char strHrs[]         = "hrs";
const char strMins[]        = "mins";

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Access Point Active";
const char strWifiReset[]  = "WiFi reset in 5 seconds";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Printer Settings";
const char wcSecWifi[]          = "WiFi Control";
const char wcPrinterIP[]        = "Printer IP";
const char wcPrinterPort[]      = "Printer Port";
const char wcClock24[]          = "24 Hour Clock";
const char wcSaveBtn[]          = "Update &amp; Save";
const char wcWifiResetBtn[]     = "&#x26A0; Reset WiFi";
const char wcWifiResetTitle[]   = "&#x26A0; Reset WiFi?";
const char wcWifiResetBody[]    = "Erases saved WiFi credentials and reboots into setup mode. Reconnect via the KlippyMon hotspot.";
const char wcWifiResetYes[]     = "Yes, reset";
const char wcWifiResetCancel[]  = "Cancel";
const char wcWifiResetting[]    = "Resetting WiFi...";
const char wcWifiResetHotspot[] = "Rebooting. Connect to the <strong>KlippyMon</strong> hotspot.";
const char wcSaved[]            = "&#10003; Settings saved and applied.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Push Notifications (ntfy)";
const char wcNtfyEnabled[]      = "Enable ntfy";
const char wcNtfyServer[]       = "Server URL";
const char wcNtfyTopic[]        = "Topic";
const char wcNtfyToken[]        = "Token (optional)";
const char wcNtfyStallMin[]     = "Stall Timeout (minutes)";
const char wcNtfyPort[]         = "Port (Local Host)";

#endif // LANG_EN

// =============================================================================
//  FRENCH
// =============================================================================
#ifdef LANG_FR

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Buse";
const char strProgress[]    = "Progres";
const char strBed[]         = "Lit";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Repos";
const char strPrep[]        = "PREP";

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Total";
const char strHrs[]         = "hrs";
const char strMins[]        = "mins";

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Point d'accès actif";
const char strWifiReset[]  = "Reinit. WiFi dans 5 s";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Paramètres imprimante";
const char wcSecWifi[]          = "Contrôle WiFi";
const char wcPrinterIP[]        = "IP imprimante";
const char wcPrinterPort[]      = "Port imprimante";
const char wcClock24[]          = "Horloge 24h";
const char wcSaveBtn[]          = "Enregistrer";
const char wcWifiResetBtn[]     = "&#x26A0; Réinitialiser WiFi";
const char wcWifiResetTitle[]   = "&#x26A0; Réinitialiser WiFi ?";
const char wcWifiResetBody[]    = "Efface les identifiants WiFi et redémarre en mode configuration. Reconnectez-vous via le hotspot KlippyMon.";
const char wcWifiResetYes[]     = "Oui, réinitialiser";
const char wcWifiResetCancel[]  = "Annuler";
const char wcWifiResetting[]    = "Réinitialisation WiFi...";
const char wcWifiResetHotspot[] = "Redémarrage. Connectez-vous au hotspot <strong>KlippyMon</strong>.";
const char wcSaved[]            = "&#10003; Paramètres enregistrés.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Notifications push (ntfy)";
const char wcNtfyEnabled[]      = "Activer ntfy";
const char wcNtfyServer[]       = "URL du serveur";
const char wcNtfyTopic[]        = "Sujet";
const char wcNtfyToken[]        = "Jeton (optionnel)";
const char wcNtfyStallMin[]     = "Délai de blocage (minutes)";
const char wcNtfyPort[]         = "Port (Hôte local)";

#endif // LANG_FR

// =============================================================================
//  GERMAN
// =============================================================================
#ifdef LANG_DE

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Duese";
const char strProgress[]    = "Fortsch.";   // Fortschritt truncated
const char strBed[]         = "Bett";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Frei";
const char strPrep[]        = "VOR";        // Vorbereitung

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Gesamt";
const char strHrs[]         = "Std";        // Stunden
const char strMins[]        = "Min";        // Minuten

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Zugangspunkt aktiv";
const char strWifiReset[]  = "WiFi-Reset in 5 Sek.";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Druckereinstellungen";
const char wcSecWifi[]          = "WiFi-Steuerung";
const char wcPrinterIP[]        = "Drucker IP";
const char wcPrinterPort[]      = "Drucker Port";
const char wcClock24[]          = "24-Stunden-Uhr";
const char wcSaveBtn[]          = "Speichern";
const char wcWifiResetBtn[]     = "&#x26A0; WiFi zurücksetzen";
const char wcWifiResetTitle[]   = "&#x26A0; WiFi zurücksetzen?";
const char wcWifiResetBody[]    = "Löscht WLAN-Daten und startet im Konfigurationsmodus neu. Verbinde mit dem KlippyMon Hotspot.";
const char wcWifiResetYes[]     = "Ja, zurücksetzen";
const char wcWifiResetCancel[]  = "Abbrechen";
const char wcWifiResetting[]    = "WiFi wird zurückgesetzt...";
const char wcWifiResetHotspot[] = "Neustart. Verbinde mit dem Hotspot <strong>KlippyMon</strong>.";
const char wcSaved[]            = "&#10003; Einstellungen gespeichert.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Push-Benachrichtigungen (ntfy)";
const char wcNtfyEnabled[]      = "ntfy aktivieren";
const char wcNtfyServer[]       = "Server-URL";
const char wcNtfyTopic[]        = "Thema";
const char wcNtfyToken[]        = "Token (optional)";
const char wcNtfyStallMin[]     = "Stillstandszeit (Minuten)";
const char wcNtfyPort[]         = "Port (Lokaler Host)";

#endif // LANG_DE

// =============================================================================
//  SPANISH
// =============================================================================
#ifdef LANG_ES

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Boquil.";    // Boquilla truncated
const char strProgress[]    = "Progreso";
const char strBed[]         = "Cama";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Libre";
const char strPrep[]        = "PREP";

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Total";
const char strHrs[]         = "hrs";
const char strMins[]        = "mins";

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Punto de acceso activo";
const char strWifiReset[]  = "Reinicio WiFi en 5 s";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Configuración impresora";
const char wcSecWifi[]          = "Control WiFi";
const char wcPrinterIP[]        = "IP impresora";
const char wcPrinterPort[]      = "Puerto impresora";
const char wcClock24[]          = "Reloj de 24 horas";
const char wcSaveBtn[]          = "Guardar";
const char wcWifiResetBtn[]     = "&#x26A0; Restablecer WiFi";
const char wcWifiResetTitle[]   = "&#x26A0; Restablecer WiFi?";
const char wcWifiResetBody[]    = "Borra las credenciales WiFi y reinicia en modo configuración. Reconéctese al hotspot KlippyMon.";
const char wcWifiResetYes[]     = "Sí, restablecer";
const char wcWifiResetCancel[]  = "Cancelar";
const char wcWifiResetting[]    = "Restableciendo WiFi...";
const char wcWifiResetHotspot[] = "Reiniciando. Conéctese al hotspot <strong>KlippyMon</strong>.";
const char wcSaved[]            = "&#10003; Configuración guardada.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Notificaciones push (ntfy)";
const char wcNtfyEnabled[]      = "Activar ntfy";
const char wcNtfyServer[]       = "URL del servidor";
const char wcNtfyTopic[]        = "Tema";
const char wcNtfyToken[]        = "Token (opcional)";
const char wcNtfyStallMin[]     = "Tiempo de espera (minutos)";
const char wcNtfyPort[]         = "Puerto (Host local)";

#endif // LANG_ES

// =============================================================================
//  DUTCH
// =============================================================================
#ifdef LANG_NL

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Nozzle";
const char strProgress[]    = "Voortg.";    // Voortgang truncated
const char strBed[]         = "Bed";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Vrij";
const char strPrep[]        = "PREP";

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Totaal";
const char strHrs[]         = "uur";
const char strMins[]        = "min";

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Toegangspunt actief";
const char strWifiReset[]  = "WiFi-reset over 5 sec.";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Printerinstellingen";
const char wcSecWifi[]          = "WiFi-beheer";
const char wcPrinterIP[]        = "Printer IP";
const char wcPrinterPort[]      = "Printer poort";
const char wcClock24[]          = "24-uursklok";
const char wcSaveBtn[]          = "Opslaan";
const char wcWifiResetBtn[]     = "&#x26A0; WiFi resetten";
const char wcWifiResetTitle[]   = "&#x26A0; WiFi resetten?";
const char wcWifiResetBody[]    = "Verwijdert WiFi-gegevens en herstart in configuratiemodus. Verbind opnieuw via de KlippyMon hotspot.";
const char wcWifiResetYes[]     = "Ja, resetten";
const char wcWifiResetCancel[]  = "Annuleren";
const char wcWifiResetting[]    = "WiFi wordt gereset...";
const char wcWifiResetHotspot[] = "Herstarten. Verbind met het <strong>KlippyMon</strong> hotspot.";
const char wcSaved[]            = "&#10003; Instellingen opgeslagen.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Pushmeldingen (ntfy)";
const char wcNtfyEnabled[]      = "ntfy inschakelen";
const char wcNtfyServer[]       = "Server URL";
const char wcNtfyTopic[]        = "Onderwerp";
const char wcNtfyToken[]        = "Token (optioneel)";
const char wcNtfyStallMin[]     = "Wachttijd (minuten)";
const char wcNtfyPort[]         = "Poort (Lokale host)";

#endif // LANG_NL

// =============================================================================
//  PORTUGUESE
// =============================================================================
#ifdef LANG_PT

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Bocal";
const char strProgress[]    = "Progresso";
const char strBed[]         = "Cama";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Livre";
const char strPrep[]        = "PREP";

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Total";
const char strHrs[]         = "hrs";
const char strMins[]        = "mins";

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Ponto de acesso ativo";
const char strWifiReset[]  = "Reinicio WiFi em 5 s";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Configurações da impressora";
const char wcSecWifi[]          = "Controle WiFi";
const char wcPrinterIP[]        = "IP da impressora";
const char wcPrinterPort[]      = "Porta da impressora";
const char wcClock24[]          = "Relógio 24h";
const char wcSaveBtn[]          = "Salvar";
const char wcWifiResetBtn[]     = "&#x26A0; Redefinir WiFi";
const char wcWifiResetTitle[]   = "&#x26A0; Redefinir WiFi?";
const char wcWifiResetBody[]    = "Apaga credenciais WiFi e reinicia no modo configuração. Reconecte-se ao hotspot KlippyMon.";
const char wcWifiResetYes[]     = "Sim, redefinir";
const char wcWifiResetCancel[]  = "Cancelar";
const char wcWifiResetting[]    = "Redefinindo WiFi...";
const char wcWifiResetHotspot[] = "Reiniciando. Conecte-se ao hotspot <strong>KlippyMon</strong>.";
const char wcSaved[]            = "&#10003; Configurações salvas.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Notificações push (ntfy)";
const char wcNtfyEnabled[]      = "Ativar ntfy";
const char wcNtfyServer[]       = "URL do servidor";
const char wcNtfyTopic[]        = "Tópico";
const char wcNtfyToken[]        = "Token (opcional)";
const char wcNtfyStallMin[]     = "Tempo de espera (minutos)";
const char wcNtfyPort[]         = "Porta (Host local)";

#endif // LANG_PT

// =============================================================================
//  TURKISH
// =============================================================================
#ifdef LANG_TR

// ── Gauge headings ────────────────────────────────────────────────────────
const char strNozzle[]      = "Nozul";
const char strProgress[]    = "Ilerleme";
const char strBed[]         = "Tabla";

// ── Progress gauge centre (max 5 chars) ──────────────────────────────────
const char strIdle[]        = "Bosta";
const char strPrep[]        = "HAZR";       // Hazırlık

// ── Success screen ────────────────────────────────────────────────────────
const char strTotal[]       = "Toplam";
const char strHrs[]         = "saat";
const char strMins[]        = "dak";        // dakika

// ── WiFi screens ──────────────────────────────────────────────────────────
const char strAPActive[]   = "Erişim noktası aktif";
const char strWifiReset[]  = "WiFi 5 sn icinde sifirlanir";

// ── Web UI ────────────────────────────────────────────────────────────────
const char wcTitle[]            = "KlippyMon";
const char wcSecPrinter[]       = "Yazıcı Ayarları";
const char wcSecWifi[]          = "WiFi Kontrolü";
const char wcPrinterIP[]        = "Yazıcı IP";
const char wcPrinterPort[]      = "Yazıcı Portu";
const char wcClock24[]          = "24 saat formatı";
const char wcSaveBtn[]          = "Kaydet";
const char wcWifiResetBtn[]     = "&#x26A0; WiFi Sıfırla";
const char wcWifiResetTitle[]   = "&#x26A0; WiFi Sıfırla?";
const char wcWifiResetBody[]    = "WiFi bilgilerini siler ve yapılandırma modunda yeniden başlatır. KlippyMon hotspotuna bağlanın.";
const char wcWifiResetYes[]     = "Evet, sıfırla";
const char wcWifiResetCancel[]  = "İptal";
const char wcWifiResetting[]    = "WiFi sıfırlanıyor...";
const char wcWifiResetHotspot[] = "Yeniden başlatılıyor. <strong>KlippyMon</strong> hotspotuna bağlanın.";
const char wcSaved[]            = "&#10003; Ayarlar kaydedildi.";

// ── ntfy Notifications (Web UI) ──────────────────────────────────────────
const char wcSecNtfy[]          = "Anlık Bildirimler (ntfy)";
const char wcNtfyEnabled[]      = "ntfy'yi etkinleştir";
const char wcNtfyServer[]       = "Sunucu URL";
const char wcNtfyTopic[]        = "Konu";
const char wcNtfyToken[]        = "Token (isteğe bağlı)";
const char wcNtfyStallMin[]     = "Bekleme süresi (dakika)";
const char wcNtfyPort[]         = "Port (Yerel host)";

#endif // LANG_TR

#endif // TRANSLATION_H
