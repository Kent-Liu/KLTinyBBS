#pragma once
#include "SinglePortModule.h"
#include "Observer.h"

/**
 * A simple example module that just replies with "Message received" to any message it receives.
 */
class KLTinyBBSModule : public SinglePortModule
{
  public:
    /** Module version string from config.h (KLTINYBBS_MODULE_VERSION_STRING). */
    static const char* const MODULE_VERSION;

    /** Constructor
     * name is for debugging output
     */
    KLTinyBBSModule();

  protected:
    // Lifecycle hook: called once after hardware/mesh layers init.
    virtual void setup() override;

    /** For reply module we do all of our processing in the (normally optional)
     * want_replies handling
     */
    virtual meshtastic_MeshPacket *allocReply() override;

    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;

    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

    virtual bool handleCommand(const meshtastic_MeshPacket &mp);

    void sendReply(uint32_t toNode, const char* text, size_t len);
    void sendReplyStr(uint32_t toNode, const char* text);

  private:
    // Called before deep sleep / reboot to flush pending data.
    int onNotifyDeepSleep(void *unused = NULL);
    int onNotifyReboot(void *unused = NULL);

    CallbackObserver<KLTinyBBSModule, void *> notifyDeepSleepObserver =
        CallbackObserver<KLTinyBBSModule, void *>(this, &KLTinyBBSModule::onNotifyDeepSleep);
    CallbackObserver<KLTinyBBSModule, void *> notifyRebootObserver =
        CallbackObserver<KLTinyBBSModule, void *>(this, &KLTinyBBSModule::onNotifyReboot);
};