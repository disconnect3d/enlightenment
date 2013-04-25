#include "e_mod_main.h"

/* Config function protos */
static Config *_notification_cfg_new(void);
static void    _notification_cfg_free(Config *cfg);

/* Global variables */
E_Module *notification_mod = NULL;
Config *notification_cfg = NULL;

static E_Config_DD *conf_edd = NULL;

static unsigned int
_notification_notify(E_Notification_Notify *n)
{
   unsigned int new_id;
   int popuped;

   if (e_desklock_state_get()) return 0;

   notification_cfg->next_id++;
   new_id = notification_cfg->next_id;
   popuped = notification_popup_notify(n, new_id);
   if (!popuped)
     {
        n->urgency = 4;
        notification_popup_notify(n, new_id);
     }

   return new_id;
}

static void
_notification_show_common(const char *summary,
                          const char *body,
                          int         replaces_id)
{
   E_Notification_Notify n;
   memset(&n, 0, sizeof(E_Notification_Notify));
   n.app_name = "enlightenment";
   n.replaces_id = replaces_id;
   n.icon.icon = "enlightenment";
   n.sumary = summary;
   n.body = body;
   e_notification_client_send(&n, NULL, NULL);
}

static void
_notification_show_presentation(Eina_Bool enabled)
{
   const char *summary, *body;

   if (enabled)
     {
        summary = _("Enter Presentation Mode");
        body = _("Enlightenment is in <b>presentation</b> mode."
                 "<br>During presentation mode, screen saver, lock and "
                 "power saving will be disabled so you are not interrupted.");
     }
   else
     {
        summary = _("Exited Presentation Mode");
        body = _("Presentation mode is over."
                 "<br>Now screen saver, lock and "
                 "power saving settings will be restored.");
     }

   _notification_show_common(summary, body, -1);
}

static void
_notification_show_offline(Eina_Bool enabled)
{
   const char *summary, *body;

   if (enabled)
     {
        summary = _("Enter Offline Mode");
        body = _("Enlightenment is in <b>offline</b> mode.<br>"
                 "During offline mode, modules that use network will stop "
                 "polling remote services.");
     }
   else
     {
        summary = _("Exited Offline Mode");
        body = _("Now in <b>online</b> mode.<br>"
                 "Now modules that use network will "
                 "resume regular tasks.");
     }

   _notification_show_common(summary, body, -1);
}

static Eina_Bool
_notification_cb_config_mode_changed(Config *m_cfg,
                                     int   type __UNUSED__,
                                     void *event __UNUSED__)
{
   if (m_cfg->last_config_mode.presentation != e_config->mode.presentation)
     {
        m_cfg->last_config_mode.presentation = e_config->mode.presentation;
        _notification_show_presentation(e_config->mode.presentation);
     }

   if (m_cfg->last_config_mode.offline != e_config->mode.offline)
     {
        m_cfg->last_config_mode.offline = e_config->mode.offline;
        _notification_show_offline(e_config->mode.offline);
     }

   return EINA_TRUE;
}

static Eina_Bool
_notification_cb_initial_mode_timer(Config *m_cfg)
{
   if (e_config->mode.presentation)
     _notification_show_presentation(1);
   if (e_config->mode.offline)
     _notification_show_offline(1);

   m_cfg->initial_mode_timer = NULL;
   return EINA_FALSE;
}

static Eina_List *
_notification_corner_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
    "Top left corner",
    "Top right corner",
    "Bottom left corner",
    "Bottom right corner",
   };

   for (x = 0; x <= CORNER_BR; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t*)(long)x);
        oi->current = (*(int*)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_notification_screen_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
    "Primary screen",
    "Current screen",
    "All screens",
    "Xinerama",
   };

   for (x = 0; x <= POPUP_DISPLAY_POLICY_MULTI; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t*)(long)x);
        oi->current = (*(int*)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

/* Module Api Functions */
EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Notification"};

static const E_Notification_Server_Info server_info = {
   .name = "e17",
   .vendor = "enlightenment.org",
   .version = "0.17",
   .spec_version = "1.2",
   .capabilities = { "body", "body-markup", NULL }
};

/* Callbacks */
static unsigned int
_notification_cb_notify(void *data EINA_UNUSED, E_Notification_Notify *n)
{
   return _notification_notify(n);
}

static void
_notification_cb_close(void *data EINA_UNUSED, unsigned int id)
{
   notification_popup_close(id);
}

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];
   E_Configure_Option *co;

   snprintf(buf, sizeof(buf), "%s/e-module-notification.edj", m->dir);
   /* register config panel entry */
   e_configure_registry_category_add("extensions", 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add("extensions/notification", 30, 
                                 _("Notification"), NULL,
                                 buf, e_int_config_notification_module);


   conf_edd = E_CONFIG_DD_NEW("Notification_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_VAL(D, T, show_low, INT);
   E_CONFIG_VAL(D, T, show_normal, INT);
   E_CONFIG_VAL(D, T, show_critical, INT);
   E_CONFIG_VAL(D, T, corner, INT);
   E_CONFIG_VAL(D, T, timeout, FLOAT);
   E_CONFIG_VAL(D, T, force_timeout, INT);
   E_CONFIG_VAL(D, T, ignore_replacement, INT);
   E_CONFIG_VAL(D, T, dual_screen, INT);

   notification_cfg = e_config_domain_load("module.notification", conf_edd);
   if (notification_cfg &&
       !(e_util_module_config_check(_("Notification Module"),
                                    notification_cfg->version,
                                    MOD_CONFIG_FILE_VERSION)))
     {
        _notification_cfg_free(notification_cfg);
        notification_cfg = NULL;
     }

   if (!notification_cfg)
     notification_cfg = _notification_cfg_new();
   /* upgrades */
   if (notification_cfg->version - (MOD_CONFIG_FILE_EPOCH * 1000000) < 1)
     {
        if (notification_cfg->dual_screen) notification_cfg->dual_screen = POPUP_DISPLAY_POLICY_MULTI;
     }
   notification_cfg->version = MOD_CONFIG_FILE_VERSION;

   /* set up the notification daemon */
   if (!e_notification_server_register(&server_info, _notification_cb_notify,
                                       _notification_cb_close, NULL))
     {
        e_util_dialog_show(_("Error during notification server initialization"),
                           _("Ensure there's no other module acting as a server"
                             " and that D-Bus is correctly installed and "
                             " running"));
        return NULL;
     }

   notification_cfg->last_config_mode.presentation = e_config->mode.presentation;
   notification_cfg->last_config_mode.offline = e_config->mode.offline;
   notification_cfg->handler = ecore_event_handler_add
         (E_EVENT_CONFIG_MODE_CHANGED, (Ecore_Event_Handler_Cb)_notification_cb_config_mode_changed,
         notification_cfg);
   notification_cfg->initial_mode_timer = ecore_timer_add
       (0.1, (Ecore_Task_Cb)_notification_cb_initial_mode_timer, notification_cfg);

   notification_mod = m;

   e_configure_option_domain_current_set("notification");
   E_CONFIGURE_OPTION_ADD(co, BOOL, show_low, notification_cfg, _("Display low urgency notifications"), _("notification"));
   E_CONFIGURE_OPTION_ADD(co, BOOL, show_normal, notification_cfg, _("Display normal urgency notifications"), _("notification"));
   E_CONFIGURE_OPTION_ADD(co, BOOL, show_critical, notification_cfg, _("Display high urgency notifications"), _("notification"));
   E_CONFIGURE_OPTION_ADD(co, BOOL, force_timeout, notification_cfg, _("Force a specified timeout on all notifications"), _("notification"), _("delay"));
   E_CONFIGURE_OPTION_ADD(co, DOUBLE, timeout, notification_cfg, _("Timeout to force on notifications"), _("notification"), _("delay"));
   E_CONFIGURE_OPTION_MINMAX_STEP_FMT(co, 0.0, 15.0, 0.1, _("%.1f seconds"));
   E_CONFIGURE_OPTION_ADD(co, ENUM, dual_screen, notification_cfg, _("Screen(s) on which to display notifications"), _("notification"), _("screen"));
   co->info_cb = _notification_screen_info_cb;
   E_CONFIGURE_OPTION_ICON(co, buf);
   E_CONFIGURE_OPTION_ADD(co, ENUM, corner, notification_cfg, _("Corner in which to display notifications"), _("notification"), _("screen"));
   co->info_cb = _notification_corner_info_cb;
   E_CONFIGURE_OPTION_ICON(co, buf);

   e_configure_option_category_tag_add(_("screen"), _("notification"));
   e_configure_option_category_tag_add(_("notification"), _("notification"));
   e_configure_option_category_icon_set(_("notification"), buf);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   if (notification_cfg->initial_mode_timer)
     ecore_timer_del(notification_cfg->initial_mode_timer);

   if (notification_cfg->handler)
     ecore_event_handler_del(notification_cfg->handler);

   if (notification_cfg->cfd) e_object_del(E_OBJECT(notification_cfg->cfd));
   e_configure_registry_item_del("extensions/notification");
   e_configure_registry_category_del("extensions");

   notification_popup_shutdown();
   e_notification_server_unregister();


   e_configure_option_domain_clear("notification");
   e_configure_option_category_tag_del(_("screen"), _("notification"));
   e_configure_option_category_tag_del(_("notification"), _("notification"));
   _notification_cfg_free(notification_cfg);
   E_CONFIG_DD_FREE(conf_edd);
   notification_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return e_config_domain_save("module.notification", conf_edd, notification_cfg);
}

static Config *
_notification_cfg_new(void)
{
   Config *cfg;

   cfg = E_NEW(Config, 1);
   cfg->cfd = NULL;
   cfg->version = MOD_CONFIG_FILE_VERSION;
   cfg->show_low = 0;
   cfg->show_normal = 1;
   cfg->show_critical = 1;
   cfg->timeout = 5.0;
   cfg->force_timeout = 0;
   cfg->ignore_replacement = 0;
   cfg->dual_screen = POPUP_DISPLAY_POLICY_FIRST;
   cfg->corner = CORNER_TR;

   return cfg;
}

static void
_notification_cfg_free(Config *cfg)
{
   free(cfg);
}
