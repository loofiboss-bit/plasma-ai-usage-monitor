# Plasma AI Usage Monitor v16.0.0 — Control Loop

## Sammanfattning och nulägesreview

V16 bör fokusera på en sammanhängande kontrollslinga: **se vad som kräver uppmärksamhet → förstå varför → åtgärda eller fördjupa sig**. Rekommendationen är inte fler generella dashboards, utan högre datakorrekthet, mindre upprepning, källdetaljer och ett konkret lyft av Anthropic-integrationen.

Nuvarande grund är stark:

- Ren `main`, synkroniserad med `origin/main`.
- `just test`: 51/51 godkända.
- `just check` och full offscreen-smoke godkända.
- Typade `DailyStateModel`/`SourceReadinessModel`, sanningsenlig hantering av noll kontra otillgängligt, asynkron historik/Analyst, KWallet, SQLite v4 och lokalt dataskydd.
- 18 leverantörer och 7 prenumerationsverktyg utan telemetri eller molnbackend.

Prioriterade problem:

1. **Korrekthet:** Analysts 30-dagarsperiod visas som 31 dagar på grund av lokal tidszon som sedan konverteras till UTC.
2. **Releaseintegritet:** mediakontrollen accepterade en bild av fel programfönster. Panelbilden innehåller dessutom värddatorns skrivbordsinnehåll trots avsedd isolering.
3. **UI-struktur:** Overview upprepar samma quota/reset-information i header, specialkort och källrad. Det saknas en naturlig drill-down per källa.
4. **History/Analyst:** klippta axeltexter, tvetydig “Detail”-kontroll och svag responsivitet; Analyst staplar KPI-kort vertikalt även på bred yta.
5. **Kvalitetsgrindar:** `qmllint` går grönt trots stor varningsmängd. `SECURITY.md` säger fortfarande att 13.x är aktuell, och V15-planen står kvar som “Proposed”.
6. **Underutnyttjad integration:** koden påstår att Anthropic saknar Usage/Cost API. Anthropic har nu officiella organisationsendpoints för användning och kostnader via separat Admin API key.
7. **Underhållbarhet:** flera centrala filer är 800–3 300 rader, leverantörsregistreringen är delvis manuell, konfigurationen har 215 nycklar och `dashboardMode`/`showOnlyProblems` är döda kontrakt.

### Visuell evidens från aktuell körning

| Overview-tillstånd | History-tillstånd | Analyst och panel |
|---|---|---|
| ![Nuvarande Overview](/tmp/plasma-ai-usage-monitor-v16-audit-2/overview-popup.png)<br>![Attention state](/tmp/plasma-ai-usage-monitor-v16-audit-2/attention-state.png)<br>![Quota reset state](/tmp/plasma-ai-usage-monitor-v16-audit-2/quota-reset-state.png)<br>![Tool-only state](/tmp/plasma-ai-usage-monitor-v16-audit-2/tool-only-overview.png) | ![Retained history](/tmp/plasma-ai-usage-monitor-v16-audit-2/retained-history.png)<br>![History gap](/tmp/plasma-ai-usage-monitor-v16-audit-2/history-gap.png) | ![Analyst sufficient data](/tmp/plasma-ai-usage-monitor-v16-audit-2/analyst-sufficient.png)<br>![Analyst insufficient data](/tmp/plasma-ai-usage-monitor-v16-audit-2/analyst-insufficient.png)<br>![Compact panel](/tmp/plasma-ai-usage-monitor-v16-audit-2/panel-lowest-quota.png) |

State coverage omfattar healthy, attention, quota/reset, tool-only, retained history, datagap, tillräcklig/otillräcklig Analyst-data och panelrepresentation. Onboarding, Providers Settings och Diagnostics granskades i kod men saknar aktuell visuell evidens; V16:s mediakörning ska därför inkludera dem.

UI-riktningen följer KDE:s rekommendationer om `NavigationTabBar` för få huvuddestinationer, responsiva `CardsLayout`, sparsamma men handlingsbara statusmeddelanden och textetiketter när ikonen inte är självklar. Den ska också verifieras med tangentbord och synlig fokusindikering. [KDE Layout and Navigation](https://develop.kde.org/hig/layout_and_nav/), [KDE Cards](https://develop.kde.org/docs/getting-started/kirigami/components-card/), [KDE Status Changes](https://develop.kde.org/hig/status_changes/), [KDE Icons](https://develop.kde.org/hig/icons/), [KDE Accessibility](https://develop.kde.org/hig/accessibility/).

## Genomförande

### Fas 0 — Kontrakt och mätbar V16-baslinje

- Skapa den kanoniska V16-planen och markera V15-planen som implementerad/historisk. Synkronisera `ROADMAP`, säkerhetspolicy och versionsdokumentation.
- Dokumentera aktuell testmängd, QML-varningar, prestanda, skärmbilder och de största arkitekturytorna innan beteende ändras.
- Lås följande kompatibilitetsgränser: package-id, QML URI, KWallet-folder, befintliga hemligheter, SQLite-historik, actual/estimated/fixed-fee-separation, currency-kontrakt och otillgängligt kontra numeriskt noll.
- Behåll SQLite schema v4. Befintliga `model_scope` och `project_scope` räcker för Anthropic-modell och workspace; ingen datamigrering behövs.
- Ta bort `dashboardMode` och `showOnlyProblems` från den aktiva KConfig-definitionen. Äldre config-import ska acceptera och ignorera dem, och export ska inte längre skriva dem.

### Fas 1 — Korrekthet och tillförlitliga kvalitetsgrindar

- Ändra Analyst till en entydig UTC-period `[fromInclusive, toExclusive)`. Alla 7/30/90-dagarsintervall ska alltid innehålla exakt valt antal UTC-kalenderdagar oberoende av systemets tidszon eller sommartid.
- Rätta `requestedDayCount`, periodtext, rapportexport och SQL-villkor till samma halvt öppna kontrakt.
- Konfigurera `qmllint` med korrekta importvägar och typinformation. Legitima kontextobjekt deklareras explicit; varningar får inte globalt stängas av. V16-grinden använder maskinläsbart resultat och `--max-warnings 0`. [Qt qmllint](https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html)
- Gör release-media semantiskt verifierbar:
  - kör med temporärt HOME/XDG, tom Desktop och kontrollerad bakgrund;
  - fånga endast förväntat PID/window-id;
  - verifiera ett viewspecifikt AT-SPI-element före och efter bilden;
  - lagra view, state, PID/window-id och förväntad accessible-marker i manifestet;
  - avslå fel aktivt fönster och alla bilder som saknar rätt state.
- Lägg till statisk kontroll att aktuell majorversion överensstämmer mellan `VERSION`, `ROADMAP`, `SECURITY.md`, AppStream och releaseplan.

### Fas 2 — Action-first Overview och Source Detail

- Ersätt de tre egenbyggda top-knapparna med `Kirigami.NavigationTabBar`. Flytta refresh, setup och settings till en tillgänglig `ActionToolBar` med text i overflow.
- Ersätt Overview-kombinationen av statusheader, quota-kort och kostnadskort med ett enda **Daily Focus**:
  - en status och högst en primär handling;
  - lowest live quota, next reset och spend som separata kompakta fakta;
  - actual, estimated, balance och fixed fee får aldrig slås samman.
- Visa därefter källor sorterade efter attention → actual usage → estimate/balance → connectivity-only → unavailable. Varje rad visar ett primärt faktum och öppnar Source Detail.
- Bygg Source Detail som ett linjärt drill-in-läge med korrekt tillbaka-fokus. Det visar:
  - aktuell status och specifik handling, exempelvis “Add Admin key”, “Refresh” eller “Open source settings”;
  - senaste lyckade hämtning, freshness och nästa schemalagda hämtning;
  - quota windows, faktisk/uppskattad kostnad och dataproveniens;
  - kompatibel 7-dagarshistorik med gap;
  - länkar till förfiltrerad History och källans inställningar.
- Introducera en typad `SourceDetailModel` i stället för nya QML-aggregeringsloopar. Modellen tar stabilt source-id och exponerar status, actions, metrics, provenance, coverage och recent-series med availability-flaggor.
- Använd källa-specifika handlingsetiketter; generiska “Fix”-knappar och sliders-ikon tas bort.
- Alla kontroller ska ha synlig tangentbordsfokus, korrekt accessible name och träffyta via normala Kirigami-kontroller. Hover får aldrig bära unik information. [WCAG Target Size](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum)

### Fas 3 — History och Analyst som responsiva arbetsytor

- Låt History-kontroller brytas till två rader på smal yta. Byt “Detail” mot tydliga lägen “Single source” och “Compare”.
- Mät axeltexternas verkliga bredd och reservera marginaler så första/sista etiketten inte klipps. Behåll explicita gap och visa legend när flera serier är aktiva.
- Stöd deep-link från Source Detail till History med förvald source, metric och 7-dagarsperiod.
- Bygg Analysts KPI-yta med `Kirigami.CardsLayout`: tre kolumner brett, två mellanstort och en smalt. Period och coverage ligger före KPI:erna.
- Formatera pengar konsekvent och lokaliserat, exempelvis `USD 2.07`, och behåll unavailable-förklaringar, mixed-currency-stopp och actual/estimated-separation.
- Extrahera Analyst-frågor och resultatprojektion från den 3 300 rader stora databasklassen till testbara worker/query-enheter. Databasens publika signalbaserade, superseding request-beteende består.

### Fas 4 — Anthropic Actual Usage and Cost

- Uppgradera katalogen till provider schema v5 med:
  - `auth.acceptAnyCredentialSet`, där `anthropic` standard key eller `anthropic_admin` kan konfigurera källan;
  - `auth.capabilityCredentialSets`, där standard key ger connectivity och Admin key ger usage/cost;
  - standardnyckeln förblir oförändrad och skrivs aldrig över av Admin key.
- Lägg `anthropic_admin` i befintlig KWallet-folder. Settings och Guided First Success förklarar skillnaden: standardnyckel ger endast connection check; organisations-Admin key ger faktisk usage/spend.
- Implementera skrivskyddad hämtning från:
  - `/v1/organizations/usage_report/messages`;
  - `/v1/organizations/cost_report`.
- Schemalagd hämtning använder 1-dagsbuckets för innevarande och föregående UTC-dag med minst fem minuters intervall. Initial/manuell backfill hämtar högst 31 dagar. Pagination fortsätter till `has_more=false`; en säkerhetsgräns ger typed partial/unavailable, aldrig ett falskt komplett totalvärde.
- Lägg till metric kinds för cache-read och cache-creation input tokens. Metric maps får `modelScope` och `projectScope`; `recordProviderMetrics` persisterar dem i befintliga v4-kolumner och metric-identitet inkluderar dimensionerna.
- Kostnadens decimalsträng i cents parsas kontrollerat till integer micro-USD innan den konverteras till presentations-/databasvärde.
- Usage och cost får oberoende capability-status. Om cost misslyckas får usage fortfarande visas; föregående cost behålls som stale. HTTP 401/403, 429/`Retry-After`, schemafel och nätverksfel mappas till befintliga typed errors.
- Priority Tier usage markeras som täckt usage men kostnaden som ofullständig; appen får inte beräkna eller antyda en saknad Priority Tier-kostnad.
- Hemligheten får aldrig förekomma i QML-text, loggar, Diagnostics, config-export, SQLite eller release fixtures.

Anthropic anger separat Admin key, full pagination, ungefär fem minuters datafördröjning, dagliga kostnadsbuckets och att Priority Tier-kostnad inte ingår i cost-endpointen. [Anthropic Usage and Cost API](https://platform.claude.com/docs/en/manage-claude/usage-cost-api)

### Fas 5 — Underhållbarhet, dokumentation och releaseberedskap

- Extrahera endast verifierade hotspots:
  - Anthropic parser/pagination till egen testbar klass;
  - Analyst SQL/query workers ur databasklassen;
  - provider runtime-registration ur `NativeMonitor`.
- Behåll katalogen som auktoritet och ersätt kvarvarande manuella kapabilitetsantaganden med katalogdata där beteendet redan är generiskt. Ingen bred provider-omskrivning.
- Uppdatera user guide, wiki mirror, arkitekturdokument, capability matrix, privacy/security och screenshots från samma isolerade capture-run.
- V16-mediamatrisen ska omfatta Overview narrow, attention, Source Detail, History gap, Analyst sufficient/insufficient, onboarding, Providers Settings, plugin recovery och kompakt panel.
- Slutlig lokal releasekandidat ska klara alla grindar och uppgradering från en verklig V15-konfiguration, KWallet och SQLite-historik. Publicering, tagg, push, COPR och KDE Store kräver separat uttryckligt uppdrag.

OpenAI:s Usage API och GitHub Copilots nya organisationsrapporter har också rikare modell-, projekt- och användningsdimensioner, men de skjuts till en senare provider-depth-release. V16 tar Anthropic först för att hålla omfattningen sammanhängande och undvika att införa organisationsbehörigheter och NDJSON-downloadflöden på flera fronter samtidigt. [OpenAI Usage API](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage), [GitHub Copilot usage metrics](https://docs.github.com/en/rest/copilot/copilot-usage-metrics?apiVersion=2026-03-10)

## Publika gränssnitt och typer

- `ProviderBackend::MetricKind`: lägg till `CacheReadInputTokens` och `CacheCreationInputTokens`.
- Provider metric map: lägg till valfria `modelScope` och `projectScope`; availability, source, quality, period och currency förblir obligatoriska sanningsgränser.
- Provider catalog schema v5: lägg till `acceptAnyCredentialSet` och `capabilityCredentialSets`; gamla `credentialSlots` fortsätter som standardkontrakt för övriga providers.
- `SourceDetailModel`: nytt QML-exponerat typat läsgränssnitt för vald källa och dess asynkrona recent-history.
- Analyst range: befintliga `from`/`to` definieras och testas som `fromInclusive`/`toExclusive` i UTC.
- Ingen SQLite-schemaversion eller nätverkslyssnande tjänst tillkommer.

## Test- och acceptansplan

- Enhets- och kontrakttester:
  - exakt 7/30/90 dagar i UTC, Europe/Stockholm, UTC, America/New_York och över DST-gränser;
  - Anthropic pagination, tomma buckets, decimal-cents, cache tokens, modell/workspace, partiellt endpointfel, Priority Tier, 401/403, 429 och cancellation;
  - model/project-dimensioner skrivs och läses utan schemaändring;
  - standardnyckel och Admin key migreras eller raderas aldrig av varandra;
  - legacy config-import accepterar de två borttagna nycklarna utan att exportera dem igen.
- QML/UI:
  - healthy, attention, stale, unavailable, mixed currency och numeriskt noll;
  - tangentbordsnavigering Overview → Source Detail → tillbaka med återställd fokus;
  - History single/compare, gaps och okapade axlar;
  - Analyst exakt 30 dagar med 1/2/3-kolumnslayout;
  - onboarding med standard Anthropic key respektive Admin key;
  - plugin unavailable/older/newer och KWallet locked/cancelled.
- Release-media:
  - avsiktligt fel aktivt fönster måste få capture-gaten att misslyckas;
  - temporär Desktop måste vara tom;
  - alla bilder måste matcha manifestets view/state-marker och vara tagna i samma isolerade körning.
- Full verifiering:
  - `just test`
  - `just check`
  - `just qml-lint` med noll varningar
  - offscreen-smoke i 100/125/150/200 %, light/dark/high-contrast
  - `just phase7-check`
  - `just release-check`
  - Fedora 44 package/mock-install och V15→V16 upgrade-test
- Prestanda får inte överskrida befintliga Phase 7-budgetar: History 100k under 2 s, Analyst 100k under 1 s, noll kvarvarande workers och en DB-anslutning efter completion. V16 jämförs dessutom mot en ny Phase 0-baslinje på samma maskin och tillåter högst 10 % regression.

## Antaganden

- Målversion är `16.0.0` med arbetsnamnet **Control Loop**.
- Primär målgrupp är en enskild utvecklare på KDE Plasma/Fedora; ingen teamdashboard, hosted backend, telemetry eller cloud sync.
- V16 prioriterar korrekthet, källfördjupning och Anthropic framför fler providers eller nya visualiseringstyper.
- Alla V15-inställningar, KWallet-hemligheter och SQLite-data ska överleva uppgradering, förutom de två dokumenterat döda och ignorerade config-nycklarna.
- Historiska V15-noteringar användes endast som orientering; samtliga planbärande fakta verifierades mot aktuell ren `main`.
