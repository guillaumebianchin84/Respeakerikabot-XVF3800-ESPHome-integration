# Architecture Ikabot XVF3800 + ReSpeaker Lite

## Décision

- **XVF3800 au plafond** : micros, DSP XVF3800, beamforming, wake word, VAD, STT, Voice Assistant, conversation continue et LEDs.
- **ReSpeaker Lite** : unique sortie audio physique via jack vers l'enceinte Ikabot.
- Les MP3 d'activation et d'attente restent stockés localement sur le Lite.
- Le TTS est lu directement par le Lite.
- Le Lite est l'autorité sur la **fin réelle** de lecture.
- Home Assistant ne transporte pas l'audio entre les deux ESP32 : il ne transporte que les ordres, l'URL TTS et les ACK.

## Pourquoi un proxy media_player sur le XVF

Ne pas simplement supprimer `media_player` du `voice_assistant` XVF.

ESPHome utilise la présence d'une sortie locale pour maintenir l'état
`STREAMING_RESPONSE`, puis passe à `RESPONSE_FINISHED`. C'est à ce moment
qu'il consulte `continue_conversation` et rouvre le microphone si nécessaire.

Le composant `ikabot_proxy_media_player` ne décode aucun son. Il sert uniquement
de miroir d'état :

1. le XVF reçoit l'URL TTS ;
2. le proxy passe immédiatement en `ANNOUNCING` pour empêcher le timeout de
   démarrage ESPHome ;
3. Home Assistant transmet l'URL au Lite ;
4. le Lite lit réellement le TTS ;
5. le Lite renvoie START puis FIN ;
6. sur FIN, le proxy passe à `IDLE` ;
7. ESPHome Voice Assistant voit alors la fin de réponse et applique nativement
   `continue_conversation`.

Un watchdog de 70 s force la sortie si l'ACK de fin se perd.

## Séquence normale

```text
Utilisateur: "Ikabot"
        |
        v
XVF wake word
        |
        +--> baisse volume Freebox
        |
        +--> event activation_requested
                  |
                  v
            ReSpeaker Lite
            MP3 activation local
                  |
                  +--> audio_finished
                            |
                            v
                     XVF démarre VA
                            |
                       VAD / STT
                            |
          VAD END ----------+--> Lite joue MP3 attente
                            |
                      Conversation
                            |
                         TTS URI
                            |
                     proxy ANNOUNCING
                            |
                            +--> HA --> Lite
                                      |
                                  TTS réel
                                      |
                               audio_finished
                                      |
                                      v
                                proxy IDLE
                                      |
                     +----------------+----------------+
                     |                                 |
            continue_conversation=true        false
                     |                                 |
               XVF réécoute                     retour wake
```

## Cas spéciaux

- **OK_ACTION** : arrêt sans TTS et restauration du volume.
- **INCOMPRÉHENSIBLE** : arrêt silencieux et restauration du volume.
- **Activation sans ACK** : watchdog 8 s, puis démarrage de l'écoute pour ne pas
  laisser Ikabot bloqué.
- **TTS sans ACK de fin** : watchdog proxy 70 s.
- **Wake pendant TTS** : ignoré pour éviter qu'Ikabot ne s'interrompe lui-même.

## Fichiers Ikabot

- `config/ikabot-xvf3800.yaml` : configuration du futur XVF.
- `packages/ikabot-voice-assistant.yaml` : logique XVF adaptée.
- `packages/ikabot-lite-audio-node.yaml` : overlay à ajouter au Lite actuel.
- `ha/ikabot_audio_bridge.yaml` : automations Home Assistant.
- `esphome/components/ikabot_proxy_media_player/` : proxy d'état de lecture.

## Avant le premier flash

1. Ajouter au `secrets.yaml` local :
   `ikabot_xvf_api_encryption_key` et `ikabot_xvf_ota_password`.
2. Inclure `packages/ikabot-lite-audio-node.yaml` dans le YAML principal du Lite.
3. Importer les automations de `ha/ikabot_audio_bridge.yaml`.
4. Flasher le XVF avec `config/ikabot-xvf3800.yaml`.
5. Vérifier les noms des actions ESPHome créées dans Home Assistant.
6. Tester dans cet ordre : activation -> STT -> attente -> TTS -> conversation continue.
