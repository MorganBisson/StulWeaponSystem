# StulWeaponSystem — état d’avancement

Dernière mise à jour : 17 septembre 2026.

## Objectif

Construire un système d’armes réutilisable basé sur les Data Assets et le Gameplay Ability System, indépendant du type de caméra (FPS, TPS ou autre), sans reprendre les éléments spécifiques au projet Boston.

Les systèmes de mods, rareté, pickups et UI seront traités plus tard.

## Exigences de qualité

- Le plugin doit suivre les conventions et les cycles de vie d'Unreal Engine 5 ainsi que les pratiques recommandées de GAS.
- Les APIs publiques doivent être utilisables proprement en C++ et en Blueprint.
- La réplication reste autoritaire côté serveur, avec prédiction cliente uniquement lorsqu'elle est conçue et validée explicitement.
- Le code doit être performant, structuré par responsabilités et suffisamment extensible pour les besoins identifiés, sans abstraction spéculative ni overengineering.
- Les anciens systèmes de Boston servent de référence fonctionnelle : ils doivent être audités et réécrits lorsque nécessaire, pas copiés directement dans le plugin.

## Décisions d’architecture

- `AStulWeapon` est l’objet runtime complet de l’arme : mesh, ASC et AttributeSet.
- Pas de `WeaponInstance` UObject ni de Presentation Actor séparé pour le moment.
- `FStulWeaponInstanceData` contient uniquement les informations nécessaires pour restaurer une arme.
- `ItemLevel` et `GenerationSeed` restent dans l’instance data pour permettre plus tard une génération déterministe des armes et des mods ; ils ne modifient pas encore les statistiques.
- La seed servira à la génération et au diagnostic, mais les sauvegardes durables devront aussi conserver les mods et rolls résolus : une modification future de l’algorithme ou des loot tables ne doit pas transformer une arme déjà sauvegardée.
- `InitializeFromDefinition` crée une arme neuve avec l’état par défaut de sa définition ; `InitializeFromInstanceData` est réservé à la restauration d’un état existant.
- Un acteur arme ne peut être initialisé qu’une seule fois et doit déjà posséder un `Owner` valide.
- Le futur Manager devra définir l’Owner dans les paramètres de spawn ou utiliser un spawn différé avant d’appeler l’une des fonctions d’initialisation.
- `IStulWeaponOwnerInterface` fournit les données de vue et de mouvement sans imposer une architecture FPS ou TPS.
- Les inputs utilisent des Gameplay Tags et non des identifiants numériques.
- L’arme est l’avatar GAS ; son personnage propriétaire est le GAS Owner logique afin de préserver l’identification du contrôleur et la prédiction réseau.
- Le plugin ne doit jamais dépendre du module `Boston_Project`.
- Pour les tirs hitscan FPS, la trajectoire gameplay part de la caméra et suit la visée, puis elle est validée par le serveur. Une obstruction au niveau du socket `Muzzle` ne bloque ni ne redirige le tir gameplay.
- Le socket `Muzzle` reste exclusivement un point de présentation : flashes et trails y apparaissent, puis le trail rejoint progressivement la trajectoire gameplay issue de la caméra, à la manière d'Apex.

## Éléments déjà implémentés

- Types et structures communes dans `StulWeaponTypes`.
- Gameplay Tags natifs du plugin.
- Interface générique du propriétaire de l’arme.
- `UStulWeaponDefinition` et validation de ses données.
- `UStulWeaponAttributeSet` et réplication des attributs.
- Classe de base `UStulWeaponGameplayAbility`.
- `AStulWeapon` :
  - composants runtime ;
  - initialisation depuis une définition chargée ou un Primary Asset ID ;
  - chargement asynchrone des assets ;
  - application des statistiques de base ;
  - attribution des Gameplay Abilities ;
  - modes de tir ;
  - façades d’entrée par Gameplay Tags ;
  - réplication du `FPrimaryAssetId` de la définition et du mode de tir ;
  - génération d’un snapshot `FStulWeaponInstanceData`.
- `UStulWeaponManagerComponent` :
  - inventaire à slots fixes et sans Tick ;
  - collection compacte répliquée avec `FFastArraySerializer`, chaque entrée conservant son `SlotIndex` stable ;
  - callbacks Fast Array traduits en événements publics `OnWeaponAdded` et `OnWeaponRemoved` pour le gameplay et le MVVM ;
  - création et restauration des armes sous autorité serveur ;
  - réservation des slots pendant les chargements asynchrones ;
  - réplication de l’inventaire, de l’arme équipée et de la fin d’initialisation du loadout ;
  - RPC serveur validant les demandes d’équipement du client propriétaire ;
  - navigation suivante/précédente, holster et auto-équipement de la première arme ;
  - routage des Gameplay Tags d’input vers l’arme équipée ;
  - nettoyage des armes détruites ou dont l’initialisation échoue.

## Passe de nettoyage terminée

Fichiers réfléchis ajoutés :

```text
Public/AbilitySystem/StulWeaponAbilitySystemComponent.h
Private/AbilitySystem/StulWeaponAbilitySystemComponent.cpp
```

Travail réalisé :

1. `UStulWeaponAbilitySystemComponent` indexe les specs par tag lors de `OnGiveAbility` et `OnRemoveAbility`.
2. Il collecte les handles `Pressed`, `Held` et `Released`, puis les traite via `ProcessAbilityInput` et les Generic Replicated Events de GAS.
3. `AStulWeapon` conserve de simples fonctions de façade et utilise le nouvel ASC.
4. L’AttributeSet est enregistré uniquement avec `AddAttributeSetSubobject`.
5. `TryMakeInstanceData` capture explicitement l’état persistant et refuse les appels client ou effectués avant que l’arme soit prête.
6. Les tags natifs utilisent désormais la racine globale `Stul.Weapon.*`.

## Passe de robustesse et d’optimisation

1. `BeginPlay` n’est plus considéré comme le signal de disponibilité du GAS. L’initialisation explicite appelle `InitAbilityActorInfo`, ce qui déclenche `OnAvatarSet` sur les abilities une fois le propriétaire logique connu. `OnWeaponReady` attend à la fois l’ActorInfo GAS et les assets runtime.
2. L’initialisation est à usage unique et son booléen indique correctement si la demande a démarré.
3. `AvailableFireModes` utilise un `FGameplayTagContainer` et représente uniquement l’ensemble des modes supportés. `GetNextFireMode` utilise séparément l’ordre canonique `Single -> Burst -> Automatic` afin de ne pas faire dépendre le gameplay de l’ordre interne ou de l’affichage du container.
4. Les attributs de l’arme sont répliqués au propriétaire uniquement. Les Gameplay Cues et les tags servent aux informations destinées aux autres clients.
5. `CurrentAmmo` est ramené sous `MaxAmmo` lorsque le maximum diminue, y compris lors du retrait d’un Gameplay Effect persistant.
6. Le composant Manager vide ne ticke plus et `ProcessAbilityInput` ne reçoit plus de `DeltaTime` inutilisé.
7. Seul le mesh actuellement consommé par `AStulWeapon` est préchargé. Les assets réservés au tir, aux animations, à la génération et aux mods seront ajoutés aux chargements lorsqu’un système les consommera réellement.
8. Le serveur dédié ne charge pas les assets visuels de l’arme.
9. Le type `StulWeaponDefinition` est enregistré par le `DefaultGame.ini` du plugin pour `/Game` et `/StulWeaponSystem`.
10. Le serveur réplique uniquement le `FPrimaryAssetId` de la définition. Chaque client résout ensuite localement le Data Asset puis charge ses ressources visuelles de façon asynchrone.

Les weapon abilities proposent quatre politiques d’activation : `OnInputTriggered`, `WhileInputActive`, `OnSpawn` et `Manual`. Le projet appelant doit exécuter `ProcessAbilityInput` une fois après avoir collecté les inputs de la frame. Les tirs automatiques restent gérés par une ability active et un timer ; `WhileInputActive` sert à retenter l’activation lorsque ses conditions redeviennent valides. `OnSpawn` ignore les avatars en cours de destruction, torn off ou possédant une durée de vie active.

## Chargement des assets

Ne pas inclure directement `Boston_Project/Public/Utils/AssetLoadUtils.h` depuis le plugin.

Pour le moment, conserver le chargement dans `AStulWeapon`, qui possède et libère les `FStreamableHandle`. Les utilitaires existants pourront être repris sélectivement et nettoyés lorsqu’au moins deux systèmes du plugin auront réellement besoin du même comportement.

Points à éviter dans un éventuel utilitaire commun :

- chargement synchrone avec `WaitUntilComplete` dans le gameplay courant ;
- chargement asynchrone ne retournant pas son handle ;
- wrappers ne faisant qu’appeler `.Get()` sans ajouter de sécurité ou de sémantique.

## Première tranche des abilities de tir

La première implémentation de tir du plugin est consacrée au hitscan :

- `UStulWeaponFireAbility` gère les modes `Single`, `Burst` et `Automatic` dans une seule ability prédite ;
- chaque tir du client propriétaire transmet une Target Data GAS compacte contenant la vue et un numéro de séquence ;
- le serveur valide l'ordre des tirs, leur cadence, l'origine de la vue et son écart angulaire avant de consommer les munitions et d'exécuter la trace autoritaire ;
- `UStulWeaponAmmoCostEffect` retire `FireCost` de `CurrentAmmo` au moyen d'un Gameplay Effect instantané compatible avec la prédiction GAS ;
- `UStulWeaponShootingLibrary` contient les calculs sans état, les traces de visée et hitscan ainsi que la génération déterministe du spread ;
- `FStulWeaponShotResult` alimente les delegates de présentation de `AStulWeapon` et les événements Blueprint de l'ability ;
- un hit autoritaire envoie `Stul.Weapon.Event.Hit` à l'ASC éventuel de la cible, avec le `FHitResult` et les dégâts de l'arme dans le payload ;
- les données de spread dynamique ne sont pas encore calculées : cette première tranche utilise temporairement `MinSpread` comme rayon angulaire du cône ;
- l’ability propose un debug visuel désactivé par défaut montrant la visée caméra, la trace gameplay, le trajet visuel depuis le muzzle et les impacts ; les futures implémentations de traces et de volumes doivent proposer un équivalent configurable afin de rester testables sans assets visuels ;
- le coût en munitions est transmis au Gameplay Effect instantané par `Stul.Weapon.SetByCaller.Ammo` ; l’ability contrôle explicitement les munitions disponibles avant d’appliquer ce coût prédit ;
- le mode burst utilise `FireInterval` entre les projectiles, `BurstShotCount` pour la taille de la rafale et `BurstInterval` entre le dernier projectile d’une rafale et le premier de la suivante ; ce dernier délai est ramené au minimum à `FireInterval` au runtime ;
- le changement de mode passe exclusivement par `UStulWeaponChangeFireModeAbility` et l’input sémantique `Stul.Weapon.Input.ChangeFireMode` ; `CurrentFireModeTag` réplique ensuite le résultat autoritaire ;
- les projectiles, la pénétration, les dégâts fournis par un Gameplay Effect, les Gameplay Cues et la compensation de latence seront ajoutés par tranches séparées.

Le code historique de Boston reste une référence fonctionnelle. Il ne doit pas être copié directement : son aléatoire n'est pas déterministe, son projectile n'est pas strictement autoritaire et sa logique mélange gameplay, présentation, Niagara, pénétration et dépendances FPS.

## Initialisation du Manager

`InitializeDefaultLoadout` n’est volontairement pas appelé depuis `BeginPlay`. Le projet propriétaire doit l’appeler sur le serveur après que le Pawn est prêt et possédé. Cela évite d’initialiser l’ActorInfo GAS des armes avant que le contrôleur du Pawn existe. Un futur plugin FPS Ready ou une intégration Modular Gameplay peut déclencher cet appel depuis son propre init-state sans créer de dépendance dans `StulWeaponSystem`.

`OnWeaponManagerReady` est un jalon local déclenché une seule fois après le succès complet du loadout par défaut. Sur le serveur, toutes les armes attendues sont initialisées et la première arme est équipée lorsque l'auto-équipement est demandé. Sur un client, l'état de succès, la Fast Array, les références Actor, l'initialisation locale de chaque arme et la référence équipée attendue doivent tous être résolus avant le signal. Un loadout vide est valide ; une entrée invalide ou un échec d'initialisation laisse le Manager non prêt. Les changements d'inventaire effectués après ce jalon ne réinitialisent pas `IsReady()`.

## Point de reprise — 8 septembre 2026

La réplication de l’inventaire du Manager utilise maintenant une Fast Array :

- `FStulWeaponEntry` contient une arme et son `SlotIndex` stable ;
- `FStulWeaponList` ne contient que les slots occupés et utilise `FFastArraySerializer` ;
- `MarkItemDirty` est appelé à l’ajout et `MarkArrayDirty` au retrait ;
- les callbacks clients `PostReplicatedAdd`, `PreReplicatedRemove` et `PostReplicatedChange` alimentent les delegates publics existants ;
- `OnWeaponAdded`, `OnWeaponRemoved` et `OnWeaponEquipped` restent l’API publique destinée au gameplay et au MVVM ;
- le flag local `bAddedEventBroadcast` évite les doublons lorsque la référence répliquée d’un acteur arme est résolue après l’arrivée de l’entrée ;
- les caches `ObservedWeapons` et `ObservedEquippedWeapon` ont été supprimés ;
- `OnRep_EquippedWeapon` reçoit directement l’ancienne valeur répliquée ;
- `GetWeaponCount` retourne le nombre d’entrées occupées et n’utilise plus le `CountByPredicate` invalide ;
- `GetWeapons` retourne une copie compacte des armes occupées, ordonnée par index de slot ;
- le module public `NetCore` a été ajouté aux dépendances du plugin.

## Point de reprise — 9 septembre 2026

Le premier test PIE multijoueur du Manager et du routage GAS est maintenant fonctionnel :

- le loadout est créé par le serveur et les armes sont visibles sur le client propriétaire ;
- l'ajout, l'équipement, le changement d'arme et le holster sont répliqués dans le scénario testé ;
- le Controller de test collecte les inputs localement puis appelle `ProcessAbilityInput` dans `PostProcessInput` ;
- les tags d'input trouvent bien les `GameplayAbilitySpec` de l'arme équipée sur le client ;
- les abilities de test `OnInputTriggered` et `WhileInputActive` s'activent désormais côté client et côté serveur, avec les deux prints attendus.

La panne d'activation cliente venait du rôle réseau de l'Avatar GAS. Dans l'architecture actuelle, le Pawn est le GAS Owner logique mais `AStulWeapon` est l'Avatar et possède son propre ASC. L'ActorInfo était correctement initialisé sur le serveur et sur le client propriétaire, mais l'arme restait `SimulatedProxy` sur ce dernier. `UGameplayAbility::CanActivateAbility` refusait donc l'activation locale. Le Manager marque maintenant comme `AutonomousProxy` les armes appartenant à un joueur distant après leur spawn.

Cette solution est cohérente tant qu'une arme reste pilotée par le Pawn qui la possède. Si les armes peuvent plus tard être déposées, ramassées, transférées ou survivre à une nouvelle possession, leur rôle autonome et leur ActorInfo devront être remis à jour pendant ces transitions.

Les abilities de test restent en `Replication Policy: Replicate No`. C'est intentionnel : leur `Net Execution Policy` gouverne l'exécution prédite entre le client propriétaire et le serveur, tandis que la Replication Policy concerne la réplication de l'instance UObject de l'ability. La passer à `Replicate Yes` n'était pas nécessaire pour résoudre cette panne.

Les tests d'input effectués en PIE ont confirmé que les abilities de validation `OnInputTriggered` et `WhileInputActive` s'exécutent bien côté client propriétaire et côté serveur. Le développement peut reprendre sur les vraies abilities de tir.

## Reprise des abilities de tir

Les anciennes implementations Boston comprennent une base de tir commune, une spécialisation hitscan, une spécialisation projectile et une Blueprint Function Library. Leur découpage sert de point de départ, mais leur code réseau, leur dispersion aléatoire, leur autorité de spawn et leurs dépendances aux effets Boston doivent être redéfinis pour le plugin.

La prochaine tranche doit d'abord établir le contrat commun du tir : acquisition de la visée par `IStulWeaponOwnerInterface`, cadence et modes de tir, coût en munitions GAS, données de tir réseau, direction déterministe, événement de tir destiné aux systèmes de présentation, puis spécialisations hitscan et projectile. Le spread, le recoil et les effets visuels seront branchés sur ce contrat plutôt que directement couplés à l'ASC ou à l'ability concrète.

## Point de reprise — tests du tir hitscan

Les premiers tirs hitscan ont été testés en PIE depuis le client propriétaire et depuis le serveur. Les traces s’exécutent des deux côtés et la validation autoritaire reçoit les tirs du client.

Corrections et décisions appliquées après ces tests :

- le coût en munitions utilise désormais `Stul.Weapon.SetByCaller.Ammo`, avec contrôle explicite du coût par l’ability ;
- les impacts sur un acteur sans ASC, comme le sol, ne tentent plus d’envoyer `Stul.Weapon.Event.Hit` et ne produisent donc plus l’erreur `Invalid ability system component` ;
- le debug visuel configurable montre la visée caméra, les traces prédite et autoritaire, le trajet depuis le muzzle et les impacts ; son épaisseur est configurable et vaut zéro par défaut ;
- `AvailableFireModes` est maintenant un `FGameplayTagContainer`, tandis que le cycle conserve un ordre canonique séparé ;
- le client propriétaire demande le changement de mode via l’ability prédite associée à `Stul.Weapon.Input.ChangeFireMode` ; il n’existe plus de RPC métier parallèle sur `AStulWeapon` ;
- les propriétés et attributs exprimés en secondes ont été renommés `BaseFireInterval`, `BaseBurstInterval`, `FireInterval` et `BurstInterval` ; aucun Core Redirect n’est nécessaire puisque les anciens noms n’avaient pas encore été sauvegardés dans des assets ;
- les projectiles d’une rafale sont espacés par `FireInterval` et deux rafales par `max(BurstInterval, FireInterval)`.

## Point de reprise — cadence entre activations

La cadence est maintenant conservée par l'instance `UStulWeaponFireAbility` entre deux activations :

- le client local mémorise l'instant minimal du prochain tir après chaque tir effectivement soumis ;
- `CanActivateAbility` refuse une nouvelle activation locale prématurée, ce qui empêche notamment de contourner `FireInterval` en cliquant rapidement en mode `Single` ;
- le serveur ne réinitialise plus `EarliestNextAuthoritativeShotTime` au début de chaque activation, donc une nouvelle prediction key ne permet plus de contourner la validation autoritaire ;
- le dernier projectile d'une rafale conserve `BurstInterval`, tandis que les autres tirs et le mode `Single` conservent `FireInterval`.

La correction a été compilée et les tests effectués le 14 septembre 2026 n'ont révélé aucun problème : les chemins exercés des modes de tir, de la cadence persistante et de la validation réseau fonctionnent comme prévu. La matrice réseau exhaustive reste distincte de cette validation fonctionnelle courante.

## Première tranche des projectiles

Le tir projectile réutilise la même `UStulWeaponFireAbility` que le hitscan. Le `ShotType` de la Weapon Definition sélectionne uniquement la résolution finale du pipeline commun ; aucune ability hitscan ou projectile supplémentaire n'est accordée.

- `AStulWeaponProjectile` utilise une sphère de collision et `UProjectileMovementComponent` sans Tick d'acteur en fonctionnement normal ;
- seul le serveur crée le projectile gameplay et traite sa collision ainsi que l'événement `Stul.Weapon.Event.Hit` ;
- les simulated proxies désactivent leur collision et simulent la présentation à partir de données de lancement répliquées avec `COND_InitialOnly` ;
- le projectile gameplay naît à l'origine de vue validée et suit la trajectoire caméra ; `CosmeticOrigin` conserve le muzzle du tir pour la future convergence visuelle ;
- la classe projectile, sa vitesse, sa gravité, sa durée de vie, le numéro du tir et l'index du projectile sont capturés au lancement ;
- `ProjectileClass` est maintenant typé `TSoftClassPtr<AStulWeaponProjectile>` ;
- les assets gameplay et de présentation sont collectés séparément afin que le serveur dédié charge la classe projectile sans charger le mesh de l'arme ;
- le debug du projectile peut dessiner sa trajectoire réelle et son impact. Son Tick est activé uniquement lorsque `bDrawDebugTrajectory` est actif et `ENABLE_DRAW_DEBUG` disponible ;
- `UStulWeaponDefinition` demeure l'unique Data Asset : les quelques réglages balistiques restent avec l'arme, tandis que la classe projectile définit son comportement structurel. Une Projectile Definition séparée ne sera introduite que si plusieurs armes doivent partager des archetypes complexes comportant assez de données pour justifier un asset autonome.

La première compilation et le premier test PIE du projectile ont validé son spawn, son déplacement et ses collisions dans le scénario exercé. La matrice multijoueur complète reste à exécuter.

La présentation persistante utilise désormais `UProjectileVisualComponent`, un simple `USceneComponent` attaché à la collision gameplay. Il reçoit le snapshot `CosmeticOrigin` capturé au moment du tir, commence au muzzle puis converge vers la trajectoire gameplay selon une distance configurable. Son Tick est actif uniquement pendant cette convergence et reste désactivé sur serveur dédié. Les meshes et trails persistants doivent être attachés à ce composant ; les muzzle flashes, sons, tracers hitscan et impacts restent destinés aux Gameplay Cues.

## Point de reprise — composant visuel du projectile

- fichiers ajoutés : `Public/Projectiles/ProjectileVisualComponent.h` et `Private/Projectiles/ProjectileVisualComponent.cpp` ;
- `AStulWeaponProjectile` crée `VisualComponent` comme enfant de `CollisionComponent` et lui transmet `InitData.CosmeticOrigin` après avoir configuré la trajectoire gameplay ;
- la convergence utilise la distance réellement parcourue par l'acteur et un `SmoothStep`, avec `ConvergenceDistance = 1000 cm` par défaut ;
- le Tick du composant commence désactivé, s'active uniquement pendant la convergence et se coupe dès que le visuel rejoint la trajectoire ;
- aucun asset visuel n'est nécessaire pour tester : `bDrawDebugConvergence` dessine la trajectoire visuelle en violet et l'écart visuel/gameplay en cyan ;
- dans le Blueprint projectile, tout mesh ou futur trail Niagara doit être placé sous `VisualComponent`, et non directement sous `CollisionComponent` ;
- pour rendre le test évident à haute vitesse, utiliser temporairement une `ConvergenceDistance` de 3000 à 5000 cm, une durée de debug de 2 à 5 secondes et une épaisseur de ligne de 2 ;
- La cible `Boston_ProjectEditor Win64 Development` a été vérifiée le 15 septembre 2026 : UnrealBuildTool indique que la cible est à jour et termine avec succès, ce qui confirme également l'étape finale de lien.

## Prochaines étapes

1. Créer et tester les trois Gameplay Cue Notify Blueprints décrits dans le point de reprise ci-dessous, d'abord avec du debug puis avec des assets de présentation temporaires.
2. Importer le spread dynamique, puis le recoil, en les branchant sur les événements de tir sans les coupler à l’ability hitscan concrète.
3. Ajouter l’application des dégâts par Gameplay Effect et traiter ensuite la pénétration comme une tranche indépendante.
4. Réévaluer plus tard une `UStulProjectileDefinition` dédiée lorsque plusieurs types de projectiles partageront assez de données propres — collision, mouvement, durée de vie, comportement d'impact et profil de présentation — pour justifier un asset autonome.
5. Adapter le MVVM existant sans le copier directement depuis Boston :
   - créer un ViewModel de loadout alimenté par les trois delegates du Manager ;
   - initialiser les slots avec `GetMaxWeaponSlots` au lieu de la constante `2` ;
   - reconstruire l'état initial depuis le Manager afin de ne pas manquer les événements arrivés avant la création de l'UI ;
   - créer un ViewModel d'arme générique pour le nom, la description, l'icône et les attributs GAS ;
   - laisser provisoirement de côté les mods, le crosshair, le spread dynamique et les états d'aim/reload qui dépendent encore de systèmes non finalisés.
6. Placer de préférence cette couche dans un module optionnel `StulWeaponSystemUI`, dépendant de `StulWeaponSystem` et `ModelViewViewModel`, afin que le cœur runtime et les serveurs dédiés ne dépendent pas de l’UI.
7. Écrire le README d’intégration : enregistrement du Primary Asset Type, initialisation du Manager, routage des inputs et de `ProcessAbilityInput`, interface owner, MVVM et attentes réseau.
8. Auditer le découplage optionnel de GAS une fois le cœur fonctionnel stabilisé, puis introduire uniquement les interfaces réellement nécessaires pour permettre une intégration non-GAS sans dupliquer le système.
9. Terminer la matrice réseau du Manager et de la Fast Array : late join, retrait de l’arme équipée, serveur dédié, déconnexion, nouvelle possession et futurs transferts drop/pickup.
10. Décider avant distribution si le dossier `Test` reste livré comme banc de validation ou passe dans un module de tests séparé.

## Point de reprise — Gameplay Cues de tir

Le test PIE du composant visuel du projectile a été validé sur serveur et client. Une `UStulProjectileDefinition` dédiée est repoussée : les cinq réglages actuels ne justifient pas encore un nouveau type de Data Asset.

La première tranche réseau des cosmétiques est implémentée sans dépendance aux assets Boston :

- `GameplayCue.Stul.Weapon.Fire` est émis une seule fois par décharge, y compris lorsque `ShotsPerFire` produit plusieurs projectiles ; son payload fournit le muzzle dans `Location`, la direction dans `Normal`, le propriétaire dans `Instigator`, l'arme dans `EffectCauser` et la Weapon Definition dans `SourceObject` ;
- `GameplayCue.Stul.Weapon.Tracer` est émis pour chaque trajectoire hitscan ; `Location` contient le muzzle, `Normal` la direction cosmétique vers l'extrémité, `RawMagnitude` la longueur cosmétique et l'`EffectContext` contient le `HitResult` lorsqu'il existe. Le cue reconstruit exactement son extrémité avec `Location + Normal * RawMagnitude` ;
- `GameplayCue.Stul.Weapon.Impact` est émis pour les impacts hitscan prédits et autoritaires ainsi que pour les impacts projectile autoritaires ; `Location`, `Normal`, `PhysicalMaterial`, `RawMagnitude` et l'`EffectContext` fournissent respectivement le point, la normale, le matériau physique, les dégâts et le `HitResult` ;
- chaque tir client s'exécute désormais dans sa propre `FScopedPredictionWindow`. La prediction key transmise avec la Target Data permet à GAS de ne pas rejouer sur le propriétaire le cue confirmé par le serveur ;
- le projet consommateur doit enregistrer explicitement les chemins `/Game` et `/StulWeaponSystem` sous `[/Script/GameplayAbilities.AbilitySystemGlobals]` dans son `Config/DefaultGame.ini`. Une déclaration placée dans le `DefaultGame.ini` du plugin n'est pas fusionnée dans cette configuration globale ;
- la cible `Boston_ProjectEditor Win64 Development` compile et lie avec succès après ces changements.

Les assets à créer dans l'éditeur sous `Plugins/StulWeaponSystem/Content/Test/GameplayCues` sont :

1. `GCN_StulWeapon_Fire_Test`, associé à `GameplayCue.Stul.Weapon.Fire` ;
2. `GCN_StulWeapon_Tracer_Test`, associé à `GameplayCue.Stul.Weapon.Tracer` ;
3. `GCN_StulWeapon_Impact_Test`, associé à `GameplayCue.Stul.Weapon.Impact`.

Ils peuvent d'abord utiliser `Print String` ou des formes de debug. Les assets présents dans Boston, notamment `NS_BulletTracer`, `NS_Trace` et les effets de `Content/Weapons/PhysicsFX`, peuvent servir temporairement dans ces Blueprints de test mais ne doivent pas devenir des dépendances du code ou du contenu distribuable du plugin.

### Présentation configurable des armes

Une première base volontairement simple est maintenant disponible :

- `FStulWeaponPresentationData`, dans `Public/Weapons/Presentation/StulWeaponPresentationTypes.h`, regroupe les références facultatives propres au tir de l'arme : `MuzzleFlash`, `FireSound` et `MuzzleColor` ;
- `FStulWeaponTracerData` regroupe le Niagara et les paramètres `Color`, `Speed`, `Length` et `Width`. `UStulWeaponDefinition::HitscanTracer` expose ce bloc uniquement lorsque `ShotType` vaut `Hitscan`, afin de ne pas masquer les autres présentations utiles aux armes à projectile ;
- `UStulWeaponDefinition::Presentation` contient cette structure directement afin de tester le workflow sans introduire prématurément un nouveau Data Asset ou un système de fragments ;
- dans le panneau Details, `ShotType` est placé dans `Shooting|General` avant les données qu'il conditionne. Les données audiovisuelles sont affichées sous `Visual|Effects` et les structures utilisent `ShowOnlyInnerProperties`, ce qui supprime les niveaux redondants tels que `HitscanTracer > Hitscan Tracer` sans renommer les propriétés sérialisées existantes ;
- le Data Asset n'expose plus que quatre grandes familles fonctionnelles : `Initialization`, `Display`, `Shooting` et `Visual`. Les modes de tir, timings, chargeur, reload, spread, patterns, pénétration, ballistique et recoil sont des sous-catégories de `Shooting`, tandis que mesh, attachements, animation et effets Fire/Tracer sont regroupés sous `Visual` ;
- les définitions légères de munition et de profil d'impact sont des références directes ; leurs Niagara et sons restent des références souples ajoutées à `GetPresentationAssetPaths`, donc ces assets lourds sont préchargés côté client et ne sont jamais chargés par le serveur dédié ;
- `AStulWeapon::GetPresentationData` et `GetHitscanTracerData` sont des `BlueprintNativeEvent`. Leurs implémentations par défaut copient les données de la Weapon Definition, mais un projet peut les surcharger pour appliquer quelques choix runtime sans modifier les Gameplay Cues ;
- un champ vide est intentionnel et le cue doit simplement ignorer l'effet correspondant ; une référence renseignée mais absente après le préchargement reste une erreur d'initialisation de l'arme ;
- la dépendance publique `Niagara` a été ajoutée car la structure publique expose des `TSoftObjectPtr<UNiagaraSystem>` ;
- UnrealHeaderTool, la compilation du module et l'édition de liens de `Boston_ProjectEditor Win64 Development` réussissent après cette tranche.

Pour le premier test, les Gameplay Cues jouent uniquement les références configurées. Le tracer récupère l'arme via `MyTarget`, utilise le `UGameplayCueNotify_Static` natif `UStulWeaponTracerCue`, appelle `GetHitscanTracerData` et reconstruit son extrémité avec `Location + Normal * RawMagnitude`. Les soft references ont déjà été préchargées par l'arme ; aucun cue ne démarre de chargement synchrone ou asynchrone au moment du tir.

Après avoir créé le composant Niagara du muzzle, le cue `Fire` applique `MuzzleColor` avec `Set Niagara Variable (Linear Color)` sur le paramètre standard `User.MuzzleColor`. Le cue natif `Tracer` crée `NS_BulletBeam` depuis le pool Niagara, renseigne les paramètres `User.Start`, `User.Target`, `User.Color`, `User.Speed`, `User.Length` et `User.Width`, puis active le composant afin qu'aucune frame ne soit simulée avec les valeurs par défaut.

Le tracer utilise le pooling Niagara `AutoRelease`. Le système Niagara doit donc atteindre naturellement l'état `Complete` ; un système qui boucle doit être corrigé dans l'asset plutôt que masqué par un timer C++. Lorsqu'un hit bloquant existe, le payload utilise explicitement son `ImpactPoint` comme extrémité ; aucune pénétration implicite n'est appliquée.

`UStulAmmoDefinition` représente désormais le payload indépendamment du mode de déplacement choisi par l'arme. Sa première version ne contient volontairement aucun modificateur de dégâts ni donnée hitscan/projectile : elle référence seulement un `UStulImpactProfile`. `UStulWeaponDefinition::DefaultAmmo` sélectionne la munition, et `AStulWeapon::GetCurrentAmmoDefinition` constitue le point d'extension d'un éventuel changement runtime futur.

`UStulImpactProfile` regroupe un `DefaultResponse` et les `ResponsesBySurface`. Chaque réponse contient des soft references indépendantes vers un Niagara et un son ; un champ absent dans une réponse de surface hérite du champ correspondant du fallback. Le cue `Impact` récupère la munition capturée dans `SourceObject`, résout le `PhysicalMaterial` depuis ses paramètres ou le `HitResult`, puis joue la réponse déjà préchargée. Les projectiles capturent leur munition au lancement afin qu'un futur changement de munition ne modifie pas un projectile déjà en vol.

Les trois types primaires `StulWeaponDefinition`, `StulAmmoDefinition` et `StulImpactProfile` doivent être enregistrés dans l'Asset Manager du projet consommateur. Les validations éditeur signalent une arme sans munition, une munition sans profil et un profil entièrement vide.

UnrealHeaderTool et la compilation C++ `Boston_ProjectEditor Win64 Development -NoLink` de cette extension passent.

Évolutions à différer jusqu'à leur premier besoin concret :

1. introduire un `ResolvedPresentation` privé et mis en cache seulement lorsque des attachments ou mods runtime devront réellement composer plusieurs overrides ;
2. extraire une interface de provider seulement si des objets sans classe de base commune doivent fournir la présentation ;
3. séparer la structure ou créer un Presentation Profile partageable seulement si le volume de données et les usages communs le justifient ;
4. ajouter des variantes locales/distantes, le déplacement du muzzle par un accessoire et des règles de fréquence de tracer au moment où ces besoins seront implémentés.

## Point de reprise — visée et rechargement

Les abilities fondamentales de visée et de rechargement sont désormais implémentées sans reprendre les timelines et événements par frame de Boston :

- `UStulWeaponAimAbility` est une ability `LocalPredicted` maintenue tant que `Stul.Weapon.Input.Aim` reste appuyé. Elle possède `Stul.Weapon.State.Aiming`, attend la release avec `UAbilityTask_WaitInputRelease` et reste bloquée pendant un rechargement ;
- `Stul.Weapon.State.Aiming` reste l'état gameplay interne de l'ability. Un Gameplay Cue actif `GameplayCue.Stul.Weapon.Aim` transporte uniquement sa présentation vers le propriétaire et les simulated proxies, y compris lors d'une pertinence tardive. L'arme calcule `GetAimAlpha` à partir de cette transition sans réplication continue ;
- `OnAimStateChanged` et `IsAiming()` restent le contrat public Blueprint/MVVM. Les anciens Gameplay Events locaux `Aim.Start` et `Aim.End` ont été supprimés : un ViewModel doit observer le delegate de l'arme équipée puis relire l'état lors de son binding, sans dépendre d'un message éphémère ;
- `UStulWeaponAimComponent` se place sur le Character, à côté du Manager. Il suit l'arme équipée, capture sa transform de repos après attachement puis aligne localement son `AimSocket` avec la vue fournie par `IStulWeaponOwnerInterface`. Comme l'ancienne AnimInstance de Boston, la caméra et le socket sont résolus dans le même espace relatif — celui du composant d'attachement — puis le root de l'arme remplace la main IK comme transform pilotée. L'échelle de repos est intégrée au calcul de position au lieu d'être remplacée après résolution. Son Tick reste désactivé hors transition et hors visée complète ;
- le Manager expose `GetAimAlpha` et `IsFullyAimed` comme raccourcis, mais l'arme reste propriétaire de ces valeurs. Une arme déséquipée annule ses actions transitoires et remet immédiatement son alpha à zéro ;
- `FStulWeaponAimData`, conservé dans `StulWeaponTypes.h`, fournit pour l'instant `Magnification` et `AimSocketName`. `AStulWeapon::GetAimData` et `GetAimTransform` forment les points de résolution destinés à une future lunette sans coupler le plugin à une caméra FPS/TPS ;
- la caméra et l'animation du projet consommateur doivent écouter `OnAimStateChanged` puis interpoler localement. Aucun alpha de visée n'est envoyé par GAS à chaque frame ;
- `UStulWeaponReloadAbility` est `LocalPredicted`, possède `Stul.Weapon.State.Reloading`, annule tir et visée, puis utilise `UAbilityTask_WaitDelay` ;
- `State.Reloading` reste la source de vérité gameplay de l'ability. Un Gameplay Cue actif `GameplayCue.Stul.Weapon.Reload` fournit l'état public persistant, tandis que les Cues ponctuels `Reload.Commit`, `Reload.Completed` et `Reload.Cancelled` alimentent les delegates Blueprint ;
- après un délai prédit, seul le serveur applique `UStulWeaponAmmoRestoreEffect` à `CurrentAmmo`. Le client propriétaire suit localement la quantité attendue afin que les reloads incrémentaux progressent sans tenter d'appliquer un Gameplay Effect hors fenêtre de prédiction ; l'attribut autoritaire est ensuite répliqué au propriétaire ;
- le démarrage du reload est prédit immédiatement. Les commits de munitions et les Cues ponctuels correspondants sont autoritaires ; après une restauration réussie, l'ASC force une mise à jour réseau afin que l'effet instantané soit rapidement visible sur le client propriétaire ;
- une fin locale normale ne termine plus l'ability : le propriétaire conserve `State.Reloading` jusqu'à la terminaison répliquée par le serveur. Fire ne peut donc pas être prédit dans une fenêtre où le serveur considère encore le reload actif ;
- le mode `Full` attend une fois puis restaure toutes les munitions manquantes. Le mode `Custom` attend et restaure `BaseAmmoToReload` à chaque cycle jusqu'au plein ou jusqu'à une interruption ;
- démarrer un rechargement fait sortir de la visée. Aim et ChangeFireMode restent bloqués pendant le reload ; Fire refuse un reload `Full` et suit le contrat d'interruption explicite déjà défini pour un reload `Custom` ;
- `UStulWeaponAmmoRestoreEffect` applique la magnitude positive `Stul.Weapon.SetByCaller.AmmoRestore` à `CurrentAmmo`. `OnReloadCommit` est diffusé par le Cue autoritaire de chaque restauration atomique, ce qui permet de synchroniser une animation ou un son par cartouche sur tous les rôles ;
- les Cues de fin distinguent `Reload.Completed` de `Reload.Cancelled` à partir de `bWasCancelled`. Un rechargement incrémental interrompu conserve les cartouches déjà insérées ;
- les propriétés et attributs `AimSpeed` et `ReloadSpeed`, qui contenaient en réalité des secondes, sont renommés `AimDuration` et `ReloadDuration`. Les Weapon Definitions de test doivent être vérifiées et réenregistrées après recompilation ;
- aucune réserve de munitions n'est encore imposée. La future intégration devra consulter une source de munitions facultative fournie par le projet, sans lier le plugin à un inventaire particulier.

Pour tester, ajouter `UStulWeaponAimAbility` avec `Stul.Weapon.Input.Aim` et `UStulWeaponReloadAbility` avec `Stul.Weapon.Input.Reload` dans `UStulWeaponDefinition::BaseAbilities`.

La personnalisation reste différée jusqu'à stabilisation des abilities fondamentales. La direction validée est un seul futur `UStulWeaponCustomizationComponent` autoritaire pour les slots, la réplication et la sauvegarde ; GAS traitera ses contributions gameplay tandis que les données de visée et de présentation seront résolues et mises en cache sans Tick.

### Changement de mode de tir

- `UStulWeaponChangeFireModeAbility` remplace le traitement instantané spécial de `Input.ChangeFireMode` dans `AStulWeapon`. Elle est `LocalPredicted`, possède `State.ChangingFireMode`, annule un tir actif et reste bloquée pendant `State.Reloading` ;
- `FireModeChangeDuration` est un attribut GAS initialisé depuis `UStulWeaponDefinition::BaseFireModeChangeDuration`. Le changement local est prédit à la fin de ce délai, mais la reprise du tir attend la terminaison autoritaire afin de ne pas être refusée par un état serveur encore actif. Si l'ability est annulée après cette prédiction, l'arme restaure le dernier mode confirmé par réplication ;
- `UStulWeaponAbilitySystemComponent` sait désormais tester si un input sémantique reste maintenu et réactiver les abilities qui lui sont associées. Après confirmation du changement, Fire reprend avec le nouveau mode uniquement si `Input.Fire` est toujours maintenu ;
- Aim et Fire sont bloquées pendant un reload. Fire et Reload sont également bloquées pendant un changement de mode ; ChangeFireMode et Reload sont donc mutuellement exclusifs, tandis qu'un nouveau reload annule Aim et Fire ;
- `AStulWeapon::OnFireModeChangeStateChanged` permet à la présentation de lancer ou arrêter une animation générique. `OnFireModeChanged` reste l'événement de commit du nouveau mode ;
- la Weapon Definition doit accorder `UStulWeaponChangeFireModeAbility` avec `Stul.Weapon.Input.ChangeFireMode`. Sans ce mapping, cet input n'est volontairement plus traité par un raccourci hors GAS.

## Point de reprise — 16 septembre 2026

Les abilities fondamentales Fire, Aim et Reload ont été testées sur serveur et client. Les reloads `Full` et `Custom` restaurent désormais les munitions sur l'autorité puis les répliquent au propriétaire. Une fin normale s'appuie sur `bWasCancelled` pour distinguer correctement `Reload.Completed` de `Reload.Cancelled`, y compris lorsque la confirmation serveur termine une instance locale prédite.

La matrice d'interaction retenue est maintenant :

- Reload annule Aim et Fire ;
- Aim et Fire sont bloquées tant que `State.Reloading` est actif ;
- ChangeFireMode est bloquée pendant Reload et annule un Fire actif ;
- Fire et Reload sont bloquées pendant `State.ChangingFireMode` ;
- lorsque le changement de mode est confirmé, Fire reprend uniquement si `Input.Fire` est toujours maintenu ;
- une annulation après prédiction locale restaure le dernier mode confirmé par le serveur.

Avant le prochain test PIE, ouvrir la Weapon Definition de test et :

1. ajouter `UStulWeaponChangeFireModeAbility` dans `BaseAbilities` ;
2. lui associer `Stul.Weapon.Input.ChangeFireMode` ;
3. régler `BaseFireModeChangeDuration`, par exemple à `0.5 s` pour rendre la transition visible ;
4. vérifier que l'input du projet passe uniquement par le Manager et active l'ability associée à `Stul.Weapon.Input.ChangeFireMode`.

Matrice de test à reprendre :

1. changer de mode au repos et vérifier que le commit arrive après le délai ;
2. tenter Aim, Fire et ChangeFireMode pendant Reload : les trois demandes doivent être bloquées ;
3. maintenir Fire puis changer de mode : le tir doit s'arrêter, attendre, puis reprendre dans le nouveau mode ;
4. relâcher Fire pendant le délai : le tir ne doit pas reprendre ;
5. tenter Reload pendant le changement : Reload doit être refusée et la transition de mode doit continuer ;
6. répéter sur le joueur serveur et le client propriétaire ;
7. valider provisoirement les Gameplay Cues Fire, Tracer et Impact avec des `Print String` avant d'intégrer des assets visuels.

`Boston_ProjectEditor Win64 Development` a compilé et lié avec succès après cette tranche, UHT inclus.

## Point de reprise — interruption du reload Custom par Fire

Une pression sur Fire pendant un reload incrémental interrompt désormais le rechargement selon les munitions déjà disponibles, puis exécute l'action de tir demandée :

- `UStulWeaponReloadAbility` reste l'unique ability de rechargement pour éviter de dupliquer l'activation, les événements, la restauration GAS et la logique réseau de `Full` et `Custom` ;
- `UStulWeaponFireAbility` n'est plus bloquée globalement par `State.Reloading` : son `CanActivateAbility` refuse toujours un reload `Full`, mais accepte un reload `Custom` actif ;
- si au moins une munition est déjà disponible, Fire annule immédiatement Reload et le cycle d'insertion en cours ne produit aucun `Reload.Commit` ;
- si le chargeur est vide, Fire demande à Reload de terminer uniquement l'insertion en cours, attend la suppression de `State.Reloading`, puis commence le tir. Le cycle applique `AmmoRestore`, émet `Reload.Commit` et termine Reload comme annulée, ou comme complétée si cette insertion remplit le chargeur ;
- l'activation `LocalPredicted` de Fire transmet naturellement la même demande à l'instance autoritaire : le client ne force pas prématurément la fin du reload serveur et chaque côté respecte la fin de son propre cycle ;
- la cadence reste vérifiée avant l'activation de Fire. Un appui effectué avant que `FireInterval` autorise le prochain tir ne coupe donc pas Reload et n'est pas mis en attente ;
- en mode `Single`, maintenir Fire ne provoque aucune réactivation : chaque tir exige un nouvel appui valide. Lorsque le chargeur est vide, l'appui qui attend l'insertion courante reste toutefois l'unique action de tir déjà acceptée ;
- après une interruption à chargeur vide, le client peut consommer une fois la munition prédite par l'insertion terminée même si la réplication de `CurrentAmmo` n'est pas encore arrivée. Le serveur possède déjà la munition autoritaire et conserve la validation du coût ;
- les éventuels modes `Burst` et `Automatic` conservent leur comportement générique existant sans imposer ces modes aux armes à rechargement cartouche par cartouche ;
- le reload `Full` conserve son comportement bloquant et ne peut pas être interrompu par Fire.

La relation inverse est volontairement asymétrique : Reload possède désormais `State.Firing` dans ses `ActivationBlockedTags` et ne possède plus `Ability.Fire` dans ses `CancelAbilitiesWithTag`. Une demande de reload effectuée pendant une rafale ou un tir automatique actif est refusée et ne coupe pas Fire. Comme Reload utilise `OnInputTriggered`, le joueur doit appuyer de nouveau une fois Fire terminée ; aucune demande de reload n'est mise en attente implicitement.

Tests PIE à effectuer sur le joueur serveur et le client propriétaire :

1. avec au moins une munition pendant un reload `Custom`, appuyer sur Fire après la cadence : Reload et l'insertion courante doivent être annulés immédiatement, puis le tir doit partir ;
2. répéter avant la fin de `FireInterval` : Fire doit être refusée, Reload doit continuer et l'insertion doit être validée normalement ;
3. avec zéro munition, appuyer puis relâcher Fire : l'insertion courante doit produire exactement un `Reload.Commit`, puis Reload doit finir et un tir doit partir ;
4. en mode `Single`, maintenir Fire après ce tir : aucun second tir ne doit partir sans un nouvel appui ;
5. déclencher Fire juste avant et juste après un `Reload.Commit` afin de vérifier qu'aucune insertion ni aucun tir n'est dupliqué ;
6. interrompre sur la dernière insertion : Reload doit émettre `Completed`, puis Fire doit démarrer ;
7. en reload `Full`, appuyer sur Fire : Reload doit continuer et aucun tir ne doit être mis en attente ;
8. surveiller la synchronisation de `CurrentAmmo`, `Reload.Commit`, `Reload.Cancelled` ou `Reload.Completed`, ainsi que `Fire.Start` entre client et serveur.
9. pendant une rafale, appuyer sur Reload : la rafale doit se terminer sans interruption et Reload doit exiger un nouvel appui après la fin de Fire.

UnrealHeaderTool et les compilations C++ du module réussissent après cette tranche. L'édition de liens reste à relancer après fermeture de l'éditeur : `UnrealEditor-StulWeaponSystem.dll` est verrouillée par `UnrealEditor.exe` via le débogueur Rider.

## Point de reprise — création des Data Assets de munition et d'impact

Le code C++ de `UStulAmmoDefinition`, `UStulImpactProfile` et du Gameplay Cue `Impact` est en place. Les types primaires `StulAmmoDefinition` et `StulImpactProfile` sont déjà enregistrés dans `Config/DefaultGame.ini` avec les chemins `/Game` et `/StulWeaponSystem`. Aucun `.uasset` de test correspondant n'a encore été créé dans le projet.

Prochaine étape à effectuer dans l'éditeur :

1. créer un `UStulImpactProfile` de test et renseigner au minimum son `DefaultResponse` avec un Niagara, un son ou les deux ;
2. ajouter si utile une réponse propre à une Physical Surface afin de valider l'héritage indépendant de l'effet et du son depuis le fallback ;
3. créer un `UStulAmmoDefinition` de test et lui affecter ce profil dans `ImpactProfile` ;
4. affecter cette munition au champ `DefaultAmmo` de la Weapon Definition utilisée par le banc de test ;
5. sauvegarder les trois Data Assets, lancer leur validation et vérifier que leurs Primary Asset IDs sont correctement détectés ;
6. tester en PIE les impacts hitscan puis projectile, sur serveur et client propriétaire, avec au moins la surface par défaut et une surface spécialisée ;
7. confirmer que le cue joue uniquement les ressources préchargées, qu'aucun avertissement de ressource absente n'apparaît et qu'un projectile conserve la munition capturée lors de son lancement.

Ne pas enrichir encore la munition avec des multiplicateurs de dégâts, des données hitscan/projectile ou un système d'override composé : ces extensions restent différées jusqu'à ce qu'un besoin concret apparaisse pendant l'intégration.

## Vérifications en attente

- La compilation manuelle et le premier test PIE multijoueur ont validé les chemins exercés du Manager, de la Fast Array et de l'ASC. La matrice réseau complète décrite ci-dessus reste à exécuter.
- Le type `StulWeaponDefinition` doit être déclaré dans les `PrimaryAssetTypesToScan` du projet consommateur. Il a été ajouté au `Config/DefaultGame.ini` de Boston pour le banc de test ; cette exigence devra être documentée dans le README distribuable du plugin.
- `AStulWeaponTestController` traite les inputs dans `PostProcessInput` et récupère désormais le Manager aussi bien dans `OnPossess` côté serveur que dans `AcknowledgePossession` côté client. Il retente également la résolution si le composant apparaît tardivement.
- Les armes appartenant à un joueur distant sont marquées `AutonomousProxy` par le Manager après leur spawn. Sans cela, leur Avatar GAS reste `SimulatedProxy` sur le client propriétaire et `UGameplayAbility::CanActivateAbility` refuse toutes les activations locales, même avec un ActorInfo correctement local.
- Après le changement de type de `AvailableFireModes` vers `FGameplayTagContainer`, vérifier leur contenu et réenregistrer les Data Assets existants.
- Vérifier la référence `RecoilCurve` après son passage en soft reference.
- Rafraîchir les nœuds Blueprint utilisant `ProcessAbilityInput`, l’ancien `MakeInstanceData`, `Level` ou `Seed`.

## Passe de correction — 21 septembre 2026

- Toute weapon ability autre que `OnSpawn` exige désormais que son Avatar soit l'arme équipée du `UStulWeaponManagerComponent`. Le Manager reste l'unique source de vérité et le serveur applique la même condition lors de l'activation autoritaire.
- Le mode de tir ne possède plus de chemin direct `SetCurrentFireMode`, `CycleFireMode` ou `ServerCycleFireMode`. Seule `UStulWeaponChangeFireModeAbility` peut atteindre le commit privé, en conservant tags, exclusions, délai, annulation et prédiction GAS.
- `UStulWeaponAimComponent` ne ticke plus pour rechercher son Manager ou attendre un attachement. Il réagit aux delegates d'équipement, de possession, de disponibilité de l'arme et de transform du root ; le Tick ne sert plus qu'à l'interpolation visuelle. L'ancienne arme retrouve son transform hip avant un changement ou un déséquipement.
- `AStulWeapon` réinitialise son ActorInfo lors d'un changement de propriétaire ou de Controller. Le Pawn reste l'OwnerActor GAS et l'arme reste l'AvatarActor. Le rôle autonome d'une arme de joueur distant est recalculé au même endroit côté serveur.
- Les specs ne stockent plus l'arme comme `SourceObject` et la base Ability ne possède plus ce fallback : l'AvatarActor est l'unique chemin pour résoudre l'arme runtime.
- `K2_OnLocalShotPresentation` remplace l'ambigu `K2_OnShotExecuted`. Ce callback est cosmétique et limité à l'instance localement contrôlée ; le gameplay d'impact autoritaire reste dans `K2_OnAuthoritativeHit` et les Gameplay Events.
- `FGameplayAbilityTargetData_StulWeaponShot` ne déclare plus d'endpoint : elle contient uniquement l'origine de vue, la direction et la séquence. La portée continue d'être reconstruite depuis la Weapon Definition.
- Les Gameplay Cues restent à leur emplacement actuel. Leur éventuelle réorganisation de contenu doit être effectuée séparément dans l'éditeur et ne fait pas partie de cette passe de code.
- Niagara est déclaré comme plugin requis. `GameplayTasks`, `Niagara` et `PhysicsCore` sont des dépendances privées du module ; seules les dépendances présentes dans l'API publique restent publiques.
- Les champs de scaling, spread dynamique, pénétration et recoil volontairement différés apparaissent sous `Shooting|Deferred` dans les Data Assets.
- Deux tests Automation couvrent le contrat et la sérialisation de la Target Data ainsi que les calculs déterministes de direction et de muzzle. Ils passent sous `StulWeaponSystem.Shooting`.

## Deuxième passe réseau — 21 septembre 2026

- Aim, Reload et la transition de mode exposent maintenant leur état public par des Gameplay Cues actifs. Le propriétaire les démarre avec la prédiction GAS ; l'autorité les réplique aux simulated proxies et leur état actif permet une reconstruction lors d'une pertinence tardive. Les delegates Blueprint de `AStulWeapon` restent le contrat de présentation et l'API destinée à un futur ViewModel.
- Le Reload ne termine plus normalement sur l'horloge locale. Le propriétaire conserve l'ability et `State.Reloading` jusqu'à la fin autoritaire ; les restaurations de munitions, les commits de présentation et la distinction Completed/Cancelled viennent du serveur. Les interruptions explicites du reload `Custom` conservent leur flux coordonné avec Fire.
- Le serveur valide toujours l'origine et la direction client, mais reconstruit désormais l'origine effective de la trace depuis son propre viewpoint. La direction client reste acceptée à l'intérieur du cône configuré pour ne pas prétendre remplacer une future compensation de latence.
- Le cycle canonique des Fire Modes est centralisé et partagé par le runtime et la validation. Une définition valide ne peut contenir que Single, Burst et Automatic.
- La validation refuse aussi les patterns dont le nombre de points diffère de leur clé, les niveaux d'Ability invalides, les classes accordées plusieurs fois, les Input Tags inutiles et plusieurs mappings consommateurs du même input.
- `WeaponManager Ready` est maintenant un jalon local sans course avec la Fast Array : le serveur réplique un succès et le nombre d'armes attendu, puis chaque rôle attend la résolution et l'initialisation locale des Actors ainsi que l'arme auto-équipée attendue. Un échec partiel ne produit jamais Ready ; un loadout vide reste valide.
- Les tests Automation couvrent aussi la reconstruction d'une vue serveur et les invariants principaux de validation des Weapon Definitions. Leur exécution reste à effectuer dans l'Editor ou par commandlet lorsque le lancement d'Unreal est autorisé.

## Validation réseau et contrat d'impact autoritaire — 22 septembre 2026

- La comparaison angulaire entre la direction du tir et la rotation serveur instantanée a été supprimée. Les tests PIE ont montré une origine synchronisée à moins de `0,1 cm`, mais une rotation serveur retardée pouvant dépasser `50°` pendant un flick légitime. Le seuil fixe produisait donc de faux rejets.
- Le diagnostic `bLogViewValidation`, le seuil `MaxClientAimErrorDegrees` et leurs logs ont été supprimés après avoir rempli leur rôle. La validation conserve les données finies, la direction non nulle, l'origine reconstruite par le serveur, la tolérance d'origine, la séquence, la cadence, le coût et l'autorité.
- Le plugin ne choisit et n'applique aucun Gameplay Effect de dégâts. `BaseDamage` et l'attribut runtime `Damage` décrivent la puissance produite par l'arme, tandis que le projet consommateur reste responsable de la santé, des résistances, des équipes et de la mort.
- `AStulWeapon::HandleAuthoritativeHit` reste le point commun hitscan/projectile. Il exécute le cue d'impact, envoie `Stul.Weapon.Event.Hit` à l'ASC éventuel de la cible avec le `FHitResult` et la magnitude, puis diffuse `OnAuthoritativeHit` sur le serveur.
- `UStulAmmoDefinition` reste limité au payload de munition et à son profil de présentation d'impact. Il ne référence aucun Gameplay Effect propre au projet hôte.
- UnrealHeaderTool et la compilation C++ `Boston_ProjectEditor Win64 Development -NoLink` réussissent. Les tests Automation et PIE restent à effectuer dans l'éditeur.

## Convention de journalisation

- Tous les diagnostics runtime du plugin utilisent la catégorie globale `LogStulWeaponSystem`.
- `Error` signale un invariant cassé ou une opération essentielle qui a échoué.
- `Warning` signale une configuration invalide ou un appel incorrect qui demande une correction.
- `Verbose` et `VeryVerbose` servent aux états attendus et au diagnostic détaillé, notamment aux refus d’activation GAS dus aux conditions, coûts ou cooldowns.
- Aucun log répétitif ne doit être ajouté dans une boucle par frame pour un état attendu.
- Toute nouvelle fonctionnalité doit journaliser ses chemins d’erreur importants au moment de son implémentation afin d’éviter une passe de logs ultérieure.
- Les conditions `if` et les appels `UE_LOG` restent sur une seule ligne afin de respecter la convention de lisibilité du projet.
- La présentation des `.h` et `.cpp` suit les bannières de catégories définies dans `AGENTS.md`, qui constitue la référence principale des conventions de code. Ce fichier conserve seulement ce rappel, tandis que `AGENTS.md` porte la règle obligatoire.

## Correction du cycle de vie GAS — 21 septembre 2026

- `AStulWeapon::SetOwner` peut être appelé pendant le spawn avant l'enregistrement de l'ASC. Le nettoyage de l'ActorInfo est désormais ignoré tant que l'arme ne l'a pas réellement initialisé, ce qui évite le `check(AbilityActorInfo.IsValid())` de `UAbilitySystemComponent::ClearActorInfo`.
- Comme dans Lyra, une désinitialisation ne modifie l'ASC que si l'arme en est encore l'Avatar. Tant que l'Owner GAS existe, seul l'Avatar est retiré avec `SetAvatarActor(nullptr)` ; `ClearActorInfo()` est réservé à l'absence d'Owner.
- Une association Owner/Avatar déjà correcte et un changement de Controller utilisent `RefreshAbilityActorInfo()` au lieu de vider puis reconstruire l'ActorInfo.
- `Boston_ProjectEditor Win64 Development` compile et lie avec succès après cette correction. Le redémarrage PIE et les transitions possession/respawn restent à valider manuellement dans l'éditeur.

## Passe de simplification GAS et tir — 21 septembre 2026

- Le routage d'input suit désormais le modèle Lyra : les tags restent dans les `DynamicSpecSourceTags` des specs, mais l'index redondant `InputBindings` et sa maintenance ont été supprimés. Pressed, Held et Released continuent de stocker les handles résolus depuis les specs correspondantes.
- Plusieurs abilities peuvent partager un même Input Tag. La validation conserve l'interdiction d'accorder deux fois la même classe, mais ne rejette plus deux classes distinctes consommant le même input.
- La politique `OnSpawn` est centralisée dans `UStulWeaponGameplayAbility`, tentée lors de l'attribution de la spec, lors de l'installation d'un nouvel Avatar et lorsque l'arme devient Ready. Ce troisième jalon reste nécessaire car, contrairement à Lyra, l'arme attend aussi sa Definition et ses assets runtime.
- La désinitialisation GAS annule les abilities actives, vide les inputs et retire les Gameplay Cues avant de détacher l'Avatar. Elle reste protégée contre les appels précédant l'enregistrement de l'ASC et ne modifie pas un ASC déjà associé à un autre Avatar.
- Fire ne recherche plus ni ne caste l'instance de Reload. Un reload `Custom` est annulé par les tags GAS uniquement si Fire réussit son activation et son coût ; à chargeur vide, Fire échoue et le reload continue sans tir mis en attente. Un reload `Full` reste bloquant.
- Les callbacks Blueprint de l'ability `K2_OnLocalShotPresentation` et `K2_OnAuthoritativeHit` ont été supprimés. Les Gameplay Events restent le contrat gameplay/mods, les Gameplay Cues le contrat cosmétique réseau et les delegates de l'arme le contrat Blueprint local.
- Les delegates de tir sont maintenant explicites : `OnLocalFiringStarted`, `OnLocalShotExecuted` et `OnLocalFiringEnded`. `OnAuthoritativeHit` est diffusé uniquement par l'autorité après un hit confirmé.
- Hitscan et projectile utilisent le pipeline commun `AStulWeapon::HandleAuthoritativeHit`, qui exécute le Cue d'impact, envoie `Stul.Weapon.Event.Hit` à l'ASC éventuel de la cible et diffuse le delegate serveur.
- Les chemins d'assets runtime sont collectés par un helper unique, les requêtes d'état Aim/Reload/ChangeFireMode partagent le même helper et le `BeginPlay` vide de l'arme a été supprimé.
- UnrealHeaderTool ainsi que `Boston_ProjectEditor Win64 Development` compilent et lient avec succès. Les Blueprints qui utilisaient les anciens delegates ou callbacks K2 doivent être rafraîchis lors du prochain passage dans l'éditeur.

## Agrégation des tirs logiques — 21 septembre 2026

- `FStulWeaponShotExecution` représente maintenant un tir logique identifié par sa `ShotSequence` et regroupe tous ses résultats de pellets ou projectiles. Le numéro est exposé en `int32` dans cette structure Blueprint, tandis que la Target Data réseau conserve son `uint16` compact.
- `UStulWeaponFireAbility` centralise Start, Shot et End dans trois fonctions `Dispatch...`. Chacune appelle directement le handler privé de l'arme puis publie le Gameplay Event correspondant avec `UGameplayAbility::SendGameplayEvent`.
- `Event.Fire.Shot` est émis une seule fois après la boucle des projectiles. Il ne crée plus de `FGameplayAbilityTargetData_SingleTargetHit` par résultat ; les réactions individuelles aux impacts restent portées par `Event.Hit`.
- `AStulWeapon` ne s'abonne pas aux événements de son propre ASC. Ses handlers privés diffusent les delegates uniquement au propriétaire local, exécutent le Fire Cue une fois par tir logique et conservent un Tracer Cue par résultat hitscan.
- Les anciennes fonctions publiques `NotifyLocal...`, ainsi que `NotifyShotExecuted` et `SendWeaponEvent`, ont été supprimées. Le delegate `OnLocalShotExecuted` transporte désormais le tir agrégé complet.
- UnrealHeaderTool et `Boston_ProjectEditor Win64 Development` compilent et lient avec succès après ce refactor.

## Corrections projectile et validation verticale — 22 septembre 2026

- Les fonctions de publication sémantique de Fire sont renommées `DispatchFiringStarted`, `DispatchShotExecuted` et `DispatchFiringEnded` afin de distinguer clairement le producteur des handlers consommateurs de l'arme.
- La validation serveur conserve l'origine fournie par le bridge de vue, mais compare maintenant la direction cliente à `APawn::GetBaseAimRotation()`. Cette rotation inclut le pitch réseau du Pawn distant, contrairement à une Camera Component Blueprint qui peut rester horizontale sur le serveur.
- Le projectile et le root de collision de son propriétaire s'ignorent désormais réciproquement pendant le déplacement. Cette relation est retirée dans `EndPlay`, ce qui empêche le capsule sweep du tireur d'être freiné par ses propres projectiles sans empêcher ceux-ci de toucher les autres Pawns.
- Tous les `UPrimitiveComponent` placés sous `UProjectileVisualComponent` sont forcés en présentation pure : physique, overlaps et collision désactivés.
- Sur le client propriétaire, le projectile répliqué initialise sa convergence depuis le muzzle local de l'arme. L'origine gameplay, la trajectoire et le spawn restent entièrement serveur-autoritaires.
- UnrealHeaderTool et la compilation C++ `Boston_ProjectEditor Win64 Development -NoLink` réussissent. Le lien final reste à relancer après fermeture de la session Rider/LLDB qui verrouille `UnrealEditor-StulWeaponSystem.dll`.

## Prédiction locale ADS et projectile — 22 septembre 2026

- `UStulWeaponAimAbility` déclenche maintenant directement la transition de présentation sur le propriétaire local après son commit prédit. Le Gameplay Cue Aim reste le contrat réseau pour les autres rôles ; le handler de l'arme est idempotent afin que la Cue prédite ne redémarre pas la transition.
- Les tirs projectile locaux créent immédiatement une instance cosmétique de la classe projectile configurée. Cette instance est explicitement non répliquée, ne peut jamais exécuter `HandleAuthoritativeImpact` et utilise le même snapshot de direction, vitesse, gravité et durée de vie que le projectile serveur.
- `FStulWeaponProjectilePredictionKey` corrèle le proxy et le projectile autoritaire avec la prediction key d'activation GAS, la `ShotSequence` et le `ProjectileIndex`. Une petite map transitoire dans l'arme conserve uniquement les proxies en attente de confirmation.
- À la réception du projectile serveur, le propriétaire reprend la position courante du composant visuel prédit, détruit le proxy et réutilise la convergence existante pour résorber progressivement l'écart avec la trajectoire autoritaire. Les simulated proxies distants continuent d'utiliser uniquement le projectile serveur.
- Un proxy non confirmé s'autodétruit après `PredictedConfirmationTimeout`, fixé à une seconde par défaut. Sa collision reste strictement cosmétique : elle peut arrêter localement la représentation, mais elle ne publie ni hit, ni dégâts, ni Gameplay Event.
- UnrealHeaderTool et la compilation C++ `Boston_ProjectEditor Win64 Development -NoLink` réussissent. Le lien final reste bloqué uniquement par la DLL chargée dans la session Unreal/Rider active.

### Point de reprise

Le code est prêt pour la validation PIE, mais la DLL finale n'a pas pu être reliée parce que `UnrealEditor-StulWeaponSystem.dll` était encore chargée par Unreal Editor sous Rider/LLDB. À la reprise, fermer l'éditeur, compiler normalement `Boston_ProjectEditor Win64 Development`, puis lancer Dedicated Server avec deux joueurs.

Vérifier d'abord sans émulation, puis avec 100 ms de latence :

1. l'ADS doit commencer pendant la frame de l'input et conserver uniquement la durée de transition configurée par `AimDuration` ;
2. le projectile cosmétique doit apparaître immédiatement au muzzle sur le propriétaire ;
3. l'arrivée du projectile serveur ne doit produire ni second trail persistant, ni saut visible, ni double son ;
4. les tirs en strafe et pendant des flicks horizontaux ou verticaux doivent conserver leur continuité visuelle ;
5. Single, Burst, Automatic et plusieurs projectiles par tir doivent produire une clé distincte par projectile et un seul handoff correspondant ;
6. un tir contre un mur proche doit arrêter la prédiction localement, tandis que l'impact gameplay et les dégâts restent exclusivement autoritaires ;
7. avec 1 à 5 % de packet loss, un proxy jamais confirmé doit être nettoyé par `PredictedConfirmationTimeout` ;
8. le second client doit voir uniquement les projectiles serveur, sans proxy local supplémentaire.

Le principal risque visuel à observer est la réinitialisation du Niagara du Blueprint projectile lors du handoff : la position est reprise depuis le proxy, mais `K2_OnProjectileInitialized` est exécuté sur l'instance réelle. Si un bref redémarrage reste perceptible, la prochaine tranche devra traiter le transfert ou la continuité de la représentation Niagara sans modifier la simulation autoritaire.

## Reprise — 23 septembre 2026

- Unreal Editor n'était plus actif et ne verrouillait plus la DLL du plugin.
- La compilation complète `Boston_ProjectEditor Win64 Development` réussit, y compris UnrealHeaderTool et le link de `UnrealEditor-StulWeaponSystem.dll`.
- Aucun changement C++ supplémentaire n'a été nécessaire.
- Le prochain jalon reste la validation PIE Dedicated Server à deux joueurs décrite ci-dessus, d'abord sans émulation réseau puis avec 100 ms de latence et enfin avec 1 à 5 % de packet loss.

## Refonte planifiée du tir — 23 septembre 2026

Les essais PIE avec le preset réseau `Average` ont révélé une limite structurelle du tir automatique actuel : chaque projectile logique transmet une Target Data avec `ServerSetReplicatedTargetData`, qui est un RPC fiable. Sous jitter ou perte de paquets, plusieurs requêtes pourtant produites à la bonne cadence peuvent arriver groupées. La validation fondée sur leur instant d'arrivée serveur les considère alors comme trop rapides et l'échec termine toute l'ability. Cela produit des arrêts côté propriétaire, des pauses visibles par les autres rôles et un comportement très instable sous mauvaises conditions réseau.

Une file temporaire a été ajoutée à `UStulWeaponFireAbility` pour lisser ces arrivées, mais elle n'est pas retenue comme architecture finale. Son état reste lié au cycle de vie de l'ability : un `EndAbility` répliqué lors du relâchement peut terminer l'instance serveur et supprimer les tirs en attente avant leur résolution. Cette modification n'a pas été compilée à cause de Live Coding et devra être retirée avant la refonte. Le correctif séparé du Gameplay Cue Fire, qui résout le socket `Muzzle` localement sur chaque proxy au lieu d'utiliser directement la position capturée par le serveur, doit être conservé.

### Décision d'architecture

GAS reste l'orchestrateur des actions d'arme : activation prédite, tags, exclusions, coûts, attributs, états, Gameplay Effects et Gameplay Cues. Il ne doit cependant plus servir de protocole fiable haute fréquence pour chaque balle automatique. La simulation balistique, les Shot IDs, les sessions de tir, le transport spécialisé et la réconciliation doivent être placés sous la couche GAS.

La frontière retenue est la suivante :

```text
Fire Abilities
    intention, policy d'activation, permissions et durée de l'action
        |
        v
UStulWeaponFireComponent
    cadence, Shot IDs, sessions, prédiction, transport et réconciliation
        |
        v
UStulWeaponShootingLibrary
    calculs déterministes sans état, traces et données balistiques
        |
        v
AStulWeapon
    ASC, AttributeSet, Definition, delegates publics et Gameplay Cues
```

Le composant de tir exposera une primitive interne unique pour exécuter un tir logique. Un tir logique peut produire un hitscan, un projectile ou plusieurs pellets avec `ShotsPerFire`. Single demande une seule exécution ; Automatic et Burst pilotent plusieurs exécutions au moyen d'une session ou séquence. Le composant n'aura aucun Tick permanent et utilisera uniquement des timers pendant une action active.

Les statistiques ne seront pas dupliquées. `UStulWeaponDefinition` reste la source de configuration, `UStulWeaponAttributeSet` la source des valeurs runtime et `AStulWeapon` la source du mode courant. Le futur composant lit ces données ou capture un snapshot immuable au début du tir.

### Abilities et Activation Policies

`ActivationPolicy` appartient à la configuration de classe de l'ability et ne doit pas être modifiée au runtime selon le mode de tir. Plusieurs classes concrètes très fines partageront donc une base et le composant commun, sans dupliquer les traces, projectiles, coûts ou calculs :

- `UStulWeaponFireAbility` devient la base abstraite commune pour les validations GAS et l'accès au composant ;
- `UStulWeaponFireSingleAbility` utilise `OnInputTriggered` et demande un tir unique ;
- `UStulWeaponFireAutomaticAbility` utilise `WhileInputActive`, ouvre une session automatique et reste active jusqu'au relâchement ;
- `UStulWeaponFireBurstAbility` utilisera d'abord `OnInputTriggered` : un appui déclenche une rafale complète et bornée.

Une éventuelle variante de Burst répétée tant que l'input reste maintenu utilisera plus tard une classe concrète `WhileInputActive` partageant exactement la même logique de rafale. Ce choix ne doit pas être simulé en modifiant une policy au runtime.

Une Weapon Definition supportant plusieurs modes accordera plusieurs abilities avec le même `Input.Fire`. Leur `CanActivateAbility` vérifiera le mode courant afin qu'une seule soit éligible. Cette configuration conserve aussi la sémantique attendue lors d'un changement de mode pendant que Fire est maintenu : Automatic peut démarrer grâce à `WhileInputActive`, tandis que Single exige un nouvel appui.

### Hitscan et projectile

Il n'est pas prévu de créer une Fire Ability propre au hitscan et une autre propre au projectile. Ces modes diffèrent par leur simulation après acceptation du tir, pas par leurs permissions GAS ni leur input. Le composant consultera `UStulWeaponDefinition::ShotType` puis appellera le chemin hitscan ou projectile approprié.

Une ability séparée ne sera introduite que pour une sémantique d'action réellement différente, par exemple une charge maintenue puis relâchée, un beam continu, un lock-on, un tir secondaire ou une phase de placement.

### Réseau automatique à terme

L'automatique ne doit plus envoyer une Target Data fiable par balle. Sa session utilisera une identité indépendante de la Prediction Key GAS :

```text
FireSessionId + ShotSequence
```

Le protocole prévu comporte :

1. une ouverture de session rare et fiable ;
2. de petits batches de commandes `Unreliable`, avec une fenêtre glissante redondante et déduplication par séquence ;
3. une fin de session fiable contenant `FinalSequence` et les dernières commandes ;
4. un ACK cumulatif owner-only permettant de libérer l'historique prédit et de réconcilier les munitions.

La cadence serveur sera validée sur une timeline autoritaire dérivée du début de session, de `ShotSequence`, de `FireInterval` et des règles de Burst, jamais uniquement sur l'instant d'arrivée du paquet ou sur un timestamp librement choisi par le client. Les timestamps clients resteront bornés et serviront plus tard au rewind ou au catch-up projectile.

L'ACK devra pouvoir représenter les trous entre commandes acceptées et rejetées ; un simple `LastAcceptedSequence` ne suffit pas. Une forme compacte basée sur `LastProcessedSequence`, un masque récent d'acceptation et les munitions autoritaires sera évaluée. Une commande perdue ou invalide ne devra jamais annuler toute la rafale.

`CurrentAmmo` restera autoritaire dans le Weapon AttributeSet. Une éventuelle valeur affichée prédite sera dérivée des coûts encore non acquittés plutôt que stockée comme seconde source de vérité.

### Lag compensation et échelle

Le protocole de Fire Session, les ACK et la réconciliation seront stabilisés avant toute lag compensation. Le rewind hitscan et le catch-up projectile seront des extensions séparées. Le cœur du plugin ne peut pas imposer les hitboxes historiques, portes, véhicules ou règles de santé d'un projet hôte ; la compensation devra donc passer par un provider ou subsystem optionnel.

De même, `StulWeaponSystem` continue de produire un hit autoritaire et son contexte sans imposer le Gameplay Effect de santé du projet consommateur.

Les projectiles lents importants, comme les roquettes ou grenades, peuvent rester des Actors répliqués. Les balles rapides à très haute fréquence devront plus tard être évaluées comme hitscan ou simulation légère plutôt que systématiquement devenir des Actors répliqués.

### Ordre de reprise

1. Fermer Unreal Editor afin de désactiver le verrou Live Coding.
2. Retirer la file temporaire et son timer serveur de `UStulWeaponFireAbility`.
3. Conserver et compiler le correctif du muzzle local dans `UStulWeaponFireCue`.
4. Créer `UStulWeaponFireComponent` et y déplacer l'exécution d'un tir logique sans modifier son résultat fonctionnel.
5. Réduire `UStulWeaponFireAbility` à une base abstraite légère.
6. Ajouter et tester les abilities concrètes Single, Automatic et Burst avec leurs policies respectives.
7. Valider les trois modes sans émulation réseau.
8. Remplacer le transport automatique par le protocole de Fire Session spécialisé.
9. Ajouter ACK, réconciliation des munitions et tests des séquences, doublons, pertes et fermetures.
10. Exécuter la matrice PIE avec latence, jitter, packet loss, duplication et réordonnancement.
11. Ajouter seulement ensuite le rewind hitscan puis le catch-up projectile.

À la reprise, ne pas empiler de correctif supplémentaire dans l'ability actuelle. La prochaine tranche doit commencer par l'extraction du tir logique et la clarification des responsabilités ci-dessus.

## Refonte du tir — première tranche compilable

La première extraction vers le nouveau découpage est en place :

- `StulWeaponFireSessionTypes` définit les rôles d'exécution, les états de session et les identités durables `FStulShotId` / `FStulProjectileId` en `uint32` ;
- `UStulWeaponFireComponent` est un sous-composant natif répliqué de `AStulWeapon`, sans Tick permanent ;
- `ResolveExecutionRole()` distingue `PredictiveProducer`, `AuthoritativeProducer`, `AuthoritativeConsumer` et `PresentationOnly` depuis l'arme, le Pawn et son Controller ;
- le calcul du tir, les traces, le spawn des projectiles, les impacts et le debug ont quitté `UStulWeaponFireAbility` pour le composant ;
- l'ability conserve provisoirement les timers, le coût GAS et l'ancien transport `TargetData`, afin que cette tranche ne modifie pas encore le protocole réseau ;
- les anciennes Prediction Keys de projectile restent temporairement transmises au composant jusqu'à la migration vers `FStulProjectileId` et `WeaponBallisticSeed` ;
- des Automation Tests couvrent la validité, l'égalité, le hash et la représentation des nouvelles identités.

La compilation `Boston_ProjectEditor Win64 Development` réussit avec UnrealHeaderTool et le link de `UnrealEditor-StulWeaponSystem.dll`. Les Automation Tests n'ont pas été exécutés, car cela nécessiterait de lancer l'éditeur ou un commandlet.

Prochaine tranche : réduire `UStulWeaponFireAbility` à une base abstraite légère, ajouter les abilities concrètes Single/Burst/Automatic et transférer leurs timers au composant avant d'introduire les RPC de Fire Session.

## Refonte du tir — sessions réseau et présentation prédite

La migration hors du transport GAS haute fréquence est implémentée :

- `UStulWeaponFireAbility` ne possède plus de timer, de séquence ni de `TargetData` ; elle conserve le lifecycle GAS et autorise le coût de chaque tir ;
- `UStulWeaponFireSingleAbility`, `UStulWeaponFireBurstAbility` et `UStulWeaponFireAutomaticAbility` fournissent leurs policies et vérifient leur mode respectif ;
- le composant est l'unique producteur temporel et distingue explicitement `PredictiveProducer`, `AuthoritativeProducer`, `AuthoritativeConsumer` et `PresentationOnly` ;
- l'ouverture serveur attend à la fois l'Open fiable et l'autorisation de l'Ability, dans n'importe quel ordre ;
- les commandes utilisent des batches `Unreliable` redondants, une fermeture fiable avec tail, des séquences `uint32`, une fenêtre maximale de 64 et une déduplication serveur ;
- les commandes arrivées groupées sont consommées à la cadence autoritaire au lieu d'être exécutées simultanément ou de terminer l'ability ;
- les coûts Burst/Automatic sont commitées uniquement côté serveur, avec pending costs propriétaire et ACK atomiques ; Single conserve le coût GAS prédit dans sa fenêtre d'activation ;
- l'état final owner-only réconcilie les sessions même si le dernier ACK périodique est perdu ;
- `WeaponBallisticSeed` est généré par le serveur, immuable, répliqué owner-only et combiné avec `ProjectileId` ;
- les anciennes identités projectile fondées sur `PredictionKey` et le Target Data de tir ont été supprimées ;
- le projectile autoritaire reste la vérité gameplay, mais sa représentation est masquée chez le propriétaire lorsqu'un proxy prédit correspondant existe ; le proxy n'est plus détruit/remplacé ;
- le muzzle, le son et l'impact hitscan autoritaires sont distribués par RPC cosmétiques `Unreliable` en ignorant le propriétaire qui les a déjà prédits.

Les Automation Tests couvrent désormais les identités et les contrats élémentaires Batch/Close/ACK. Après fermeture d'Unreal Editor/LLDB, la compilation complète `Boston_ProjectEditor Win64 Development`, y compris l'édition de liens du plugin, réussit.

Le Blueprint de test historique dérivé directement de `UStulWeaponFireAbility` reste compatible et sélectionne le type depuis le mode courant. Les nouvelles configurations doivent accorder les trois classes concrètes avec le même Input Tag ; seule la classe correspondant au mode courant passera `CanActivateAbility`.

## Point stable — validation du tir et présentation réseau — 24 septembre 2026

- Les essais PIE à deux clients valident désormais le tir Single, Burst et Automatic après les corrections de cadence, de fermeture de session et de présentation.
- Le projectile prédit du propriétaire et le projectile autoritaire répliqué conservent leurs responsabilités distinctes. Les clients distants voient la représentation autoritaire dont le visuel démarre depuis le muzzle local de l'arme.
- Le tracer hitscan ne prend plus le muzzle capturé par le serveur dédié comme origine visuelle. Chaque client utilise le muzzle de sa représentation locale de l'arme, tout en conservant l'endpoint autoritaire encodé dans la Gameplay Cue.
- Le muzzle flash, le son et le tracer hitscan sont maintenant cohérents sur le propriétaire, le serveur d'écoute et les clients spectateurs lors des essais effectués.
- Le correctif envisagé autour de l'auto-activation Niagara du projectile a été retiré : le Blueprint avait déjà `Auto Activate` désactivé et ce mécanisme n'était pas la cause du décalage observé.
- Un test Automation couvre le contrat du tracer distant : origine au muzzle local et conservation de la cible autoritaire.
- UnrealHeaderTool et la compilation C++ `Boston_ProjectEditor Win64 Development -NoLink` réussissent. L'édition de liens complète et les Automation Tests restent à relancer après fermeture de l'éditeur et de LLDB.

### Prochaines étapes

1. Exécuter la matrice réseau complète sur Dedicated Server : sans émulation, puis avec latence, jitter, perte, duplication et réordonnancement.
2. Tester agressivement les transitions qui ont déjà révélé des régressions : reload puis Burst, relâchement/réappui rapide en Automatic, changement de mode, changement d'arme, chargeur vide et fermeture avec tail incomplète.
3. Ajouter un ViewModel MVVM de diagnostic pour exposer les munitions autoritaires, les coûts locaux encore pending, les munitions affichées et l'état de la Fire Session.
4. Nettoyer l'attribution des Fire Abilities afin de n'accorder que celles correspondant aux modes présents dans `AvailableFireModes`.
5. Exécuter les Automation Tests existants et compléter uniquement les scénarios qui échouent ou les invariants réseau non encore couverts.
6. Documenter l'intégration du plugin, notamment les Primary Asset Types, les abilities requises et la hiérarchie de présentation des projectiles.
7. Reporter le rewind hitscan, le catch-up projectile avancé et les optimisations de projectiles haute fréquence après stabilisation et profilage de cette V1.
