#include <RichHttpServer.h>
#include <MiLightClient.h>
#include <Settings.h>
#include <WebSocketsServer.h>
#include <GroupStateStore.h>
#include <RadioSwitchboard.h>
#include <PacketSender.h>
#include <TransitionController.h>
#include <AlarmController.h>

#ifndef _MILIGHT_HTTP_SERVER
#define _MILIGHT_HTTP_SERVER

#define MAX_DOWNLOAD_ATTEMPTS 3

typedef std::function<void(void)> SettingsSavedHandler;
typedef std::function<void(const BulbId& id)> GroupDeletedHandler;

using RichHttpConfig = RichHttp::Generics::Configs::EspressifBuiltin;
using RequestContext = RichHttpConfig::RequestContextType;

const char TEXT_PLAIN[] PROGMEM = "text/plain";
const char APPLICATION_JSON[] = "application/json";

class MiLightHttpServer {
public:
  MiLightHttpServer(
    Settings& settings,
    MiLightClient*& milightClient,
    GroupStateStore*& stateStore,
    PacketSender*& packetSender,
    RadioSwitchboard*& radios,
    TransitionController& transitions,
    AlarmController*& alarms
  )
    : authProvider(settings)
    , server(80, authProvider)
    , wsServer(WebSocketsServer(81))
    , numWsClients(0)
    , milightClient(milightClient)
    , settings(settings)
    , stateStore(stateStore)
    , packetSender(packetSender)
    , radios(radios)
    , transitions(transitions)
    , alarms(alarms)
  { }

  void begin();
  void handleClient();
  void onSettingsSaved(SettingsSavedHandler handler);
  void onGroupDeleted(GroupDeletedHandler handler);
  void on(const char* path, HTTPMethod method, ESP8266WebServer::THandlerFunction handler);
  void handlePacketSent(uint8_t* packet, const MiLightRemoteConfig& config);
  WiFiClient client();

protected:

  bool serveFile(const char* file, const char* contentType = "text/html");
  void handleServe_P(const char* data, size_t length);
  void sendGroupState(bool allowAsync, BulbId& bulbId, RichHttp::Response& response);

  void serveSettings();
  void handleUpdateSettings(RequestContext& request);
  void handleUpdateSettingsPost(RequestContext& request);
  void handleUpdateFile(const char* filename);

  void handleGetRadioConfigs(RequestContext& request);

  void handleAbout(RequestContext& request);
  void handleSystemPost(RequestContext& request);
  void handleFirmwareUpload();
  void handleFirmwarePost();
  void handleListenGateway(RequestContext& request);
  void handleSendRaw(RequestContext& request);

  void handleUpdateGroup(RequestContext& request);
  void handleUpdateGroupAlias(RequestContext& request);

  void handleGetGroup(RequestContext& request);
  void handleGetGroupAlias(RequestContext& request);
  void _handleGetGroup(bool allowAsync, BulbId bulbId, RequestContext& request);

  void handleDeleteGroup(RequestContext& request);
  void handleDeleteGroupAlias(RequestContext& request);
  void _handleDeleteGroup(BulbId bulbId, RequestContext& request);

  void handleGetTransition(RequestContext& request);
  void handleDeleteTransition(RequestContext& request);
  void handleCreateTransition(RequestContext& request);
  void handleListTransitions(RequestContext& request);

  void handleGetAlarm(RequestContext& request);
  void handleDeleteAlarm(RequestContext& request);
  void handleCreateAlarm(RequestContext& request);
  void handleListAlarms(RequestContext& request);
  void handleStopAlarm(RequestContext& request);
  void handleSnoozeAlarm(RequestContext& request);
  
  void handleGetPresets(RequestContext& request);
  void handlePutPresets(RequestContext& request);

  void handleGetTime(RequestContext& request);
  void handleSetTime(RequestContext& request);

  void handleRequest(const JsonObject& request);
  void handleWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

  File updateFile;

  PassthroughAuthProvider<Settings> authProvider;
  RichHttpServer<RichHttp::Generics::Configs::EspressifBuiltin> server;
  WebSocketsServer wsServer;
  size_t numWsClients;
  MiLightClient*& milightClient;
  Settings& settings;
  GroupStateStore*& stateStore;
  SettingsSavedHandler settingsSavedHandler;
  GroupDeletedHandler groupDeletedHandler;
  ESP8266WebServer::THandlerFunction _handleRootPage;
  PacketSender*& packetSender;
  RadioSwitchboard*& radios;
  TransitionController& transitions;
  AlarmController*& alarms;

};

#endif
