# StulWeaponSystem — état d’avancement

Dernière mise à jour : 15 septembre 2026.

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
3. `AvailableFireModes` utilise un `FGameplayTagContainer` et représente uniquement l’ensemble des modes supportés. `CycleFireMode` utilise séparément l’ordre canonique `Single -> Burst -> Automatic` afin de ne pas faire dépendre le gameplay de l’ordre interne ou de l’affichage du container.
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
- `CycleFireMode` accepte les appels du client propriétaire, transmet une RPC fiable au serveur puis laisse `CurrentFireModeTag` répliquer le résultat ; l’input sémantique `Stul.Weapon.Input.ChangeFireMode` emprunte le même chemin ;
- les projectiles, la pénétration, les dégâts fournis par un Gameplay Effect, les Gameplay Cues et la compensation de latence seront ajoutés par tranches séparées.

Le code historique de Boston reste une référence fonctionnelle. Il ne doit pas être copié directement : son aléatoire n'est pas déterministe, son projectile n'est pas strictement autoritaire et sa logique mélange gameplay, présentation, Niagara, pénétration et dépendances FPS.

## Initialisation du Manager

`InitializeDefaultLoadout` n’est volontairement pas appelé depuis `BeginPlay`. Le projet propriétaire doit l’appeler sur le serveur après que le Pawn est prêt et possédé. Cela évite d’initialiser l’ActorInfo GAS des armes avant que le contrôleur du Pawn existe. Un futur plugin FPS Ready ou une intégration Modular Gameplay peut déclencher cet appel depuis son propre init-state sans créer de dépendance dans `StulWeaponSystem`.

`OnWeaponManagerReady` signifie que le serveur a terminé de constituer le loadout et que cet état a été répliqué. Sur un client, les ressources visuelles d’une arme peuvent encore être en chargement ; les consommateurs nécessitant le mesh doivent aussi attendre `AStulWeapon::OnWeaponReady`.

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
- le client propriétaire peut demander `CycleFireMode` par RPC serveur, directement ou via `Stul.Weapon.Input.ChangeFireMode` ;
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

- `FStulWeaponPresentationData`, dans `Public/Weapons/Presentation/StulWeaponPresentationTypes.h`, regroupe les références facultatives communes `MuzzleFlash`, `FireSound`, `DefaultImpactEffect` et `DefaultImpactSound`, les maps `ImpactEffectsBySurface` et `ImpactSoundsBySurface`, ainsi que `MuzzleColor` ;
- `FStulWeaponTracerData` regroupe le Niagara et les paramètres `Color`, `Speed`, `Length` et `Width`. `UStulWeaponDefinition::HitscanTracer` expose ce bloc uniquement lorsque `ShotType` vaut `Hitscan`, afin de ne pas masquer les autres présentations utiles aux armes à projectile ;
- `UStulWeaponDefinition::Presentation` contient cette structure directement afin de tester le workflow sans introduire prématurément un nouveau Data Asset ou un système de fragments ;
- dans le panneau Details, `ShotType` est placé dans `Shooting|General` avant les données qu'il conditionne. Les données audiovisuelles sont affichées sous `Visual|Effects` et les structures utilisent `ShowOnlyInnerProperties`, ce qui supprime les niveaux redondants tels que `HitscanTracer > Hitscan Tracer` sans renommer les propriétés sérialisées existantes ;
- le Data Asset n'expose plus que quatre grandes familles fonctionnelles : `Initialization`, `Display`, `Shooting` et `Visual`. Les modes de tir, timings, chargeur, reload, spread, patterns, pénétration, ballistique et recoil sont des sous-catégories de `Shooting`, tandis que mesh, attachements, animation et effets Fire/Tracer/Impact sont regroupés sous `Visual` ;
- toutes les références sont souples et rejoignent `GetPresentationAssetPaths`, donc elles sont préchargées avec les ressources visuelles de l'arme et ne sont jamais chargées par le serveur dédié ;
- `AStulWeapon::GetPresentationData` et `GetHitscanTracerData` sont des `BlueprintNativeEvent`. Leurs implémentations par défaut copient les données de la Weapon Definition, mais un projet peut les surcharger pour appliquer quelques choix runtime sans modifier les Gameplay Cues ;
- un champ vide est intentionnel et le cue doit simplement ignorer l'effet correspondant ; une référence renseignée mais absente après le préchargement reste une erreur d'initialisation de l'arme ;
- la dépendance publique `Niagara` a été ajoutée car la structure publique expose des `TSoftObjectPtr<UNiagaraSystem>` ;
- UnrealHeaderTool, la compilation du module et l'édition de liens de `Boston_ProjectEditor Win64 Development` réussissent après cette tranche.

Pour le premier test, les Gameplay Cues récupèrent `MyTarget`, le castent en `AStulWeapon`, puis jouent uniquement les références configurées. Le tracer utilise le `UGameplayCueNotify_Static` natif `UStulWeaponTracerCue`, appelle `GetHitscanTracerData` et reconstruit son extrémité avec `Location + Normal * RawMagnitude`. Les soft references ont déjà été préchargées par l'arme ; le cue ne démarre aucun chargement synchrone ou asynchrone au moment du tir.

Après avoir créé le composant Niagara du muzzle, le cue `Fire` applique `MuzzleColor` avec `Set Niagara Variable (Linear Color)` sur le paramètre standard `User.MuzzleColor`. Le cue natif `Tracer` crée `NS_BulletBeam` avec le pool `AutoRelease`, renseigne les paramètres `User.Start`, `User.Target`, `User.Color`, `User.Speed`, `User.Length` et `User.Width`, puis active le composant afin qu'aucune frame ne soit simulée avec les valeurs par défaut.

Le cue `Impact` récupère le `PhysicalMaterial` depuis ses paramètres ou le `HitResult` de l'`EffectContext`, en déduit le `SurfaceType`, cherche d'abord ce type dans `ImpactEffectsBySurface` et `ImpactSoundsBySurface`, puis utilise `DefaultImpactEffect` et `DefaultImpactSound` comme fallbacks indépendants. Les traces hitscan demandaient déjà le Physical Material ; la collision projectile utilise désormais `bReturnMaterialOnMove` afin de fournir la même information.

UnrealHeaderTool et la compilation C++ de cette extension passent. La confirmation du lien reste à relancer après fermeture de l'éditeur : `UnrealEditor-StulWeaponSystem.dll` était verrouillée par `UnrealEditor.exe` via le débogueur Rider lors de la dernière tentative.

Évolutions à différer jusqu'à leur premier besoin concret :

1. introduire un `ResolvedPresentation` privé et mis en cache seulement lorsque des attachments ou mods runtime devront réellement composer plusieurs overrides ;
2. extraire une interface de provider seulement si des objets sans classe de base commune doivent fournir la présentation ;
3. séparer la structure ou créer un Presentation Profile partageable seulement si le volume de données et les usages communs le justifient ;
4. ajouter des variantes locales/distantes, le déplacement du muzzle par un accessoire et des règles de fréquence de tracer au moment où ces besoins seront implémentés.

## Point de reprise — visée et rechargement

Les abilities fondamentales de visée et de rechargement sont désormais implémentées sans reprendre les timelines et événements par frame de Boston :

- `UStulWeaponAimAbility` est une ability `LocalPredicted` maintenue tant que `Stul.Weapon.Input.Aim` reste appuyé. Elle possède `Stul.Weapon.State.Aiming`, attend la release avec `UAbilityTask_WaitInputRelease` et peut interrompre un rechargement ;
- `AStulWeapon` observe désormais directement `Stul.Weapon.State.Aiming`, qui constitue la source de vérité. Il calcule `GetAimAlpha` à partir du temps de transition sans Tick, événement par frame ou réplication continue ; `IsFullyAimed` est un état dérivé de cet alpha ;
- les Gameplay Events ponctuels `Aim.Start` et `Aim.End` sont conservés pour les consommateurs ayant besoin d'un payload. Le delegate `OnAimStateChanged` est diffusé par l'arme à partir du changement de tag, sans appel direct redondant depuis l'ability ;
- `UStulWeaponAimComponent` se place sur le Character, à côté du Manager. Il suit l'arme équipée, capture sa transform de repos après attachement puis aligne localement son `AimSocket` avec la vue fournie par `IStulWeaponOwnerInterface`. Comme l'ancienne AnimInstance de Boston, la caméra et le socket sont résolus dans le même espace relatif — celui du composant d'attachement — puis le root de l'arme remplace la main IK comme transform pilotée. L'échelle de repos est intégrée au calcul de position au lieu d'être remplacée après résolution. Son Tick reste désactivé hors transition et hors visée complète ;
- le Manager expose `GetAimAlpha` et `IsFullyAimed` comme raccourcis, mais l'arme reste propriétaire de ces valeurs. Une arme déséquipée annule ses actions transitoires et remet immédiatement son alpha à zéro ;
- `FStulWeaponAimData`, conservé dans `StulWeaponTypes.h`, fournit pour l'instant `Magnification` et `AimSocketName`. `AStulWeapon::GetAimData` et `GetAimTransform` forment les points de résolution destinés à une future lunette sans coupler le plugin à une caméra FPS/TPS ;
- la caméra et l'animation du projet consommateur doivent écouter `OnAimStateChanged` puis interpoler localement. Aucun alpha de visée n'est envoyé par GAS à chaque frame ;
- `UStulWeaponReloadAbility` est `LocalPredicted`, possède `Stul.Weapon.State.Reloading`, annule tir et visée, puis utilise `UAbilityTask_WaitDelay` ;
- `State.Reloading` est désormais la source de vérité du début de rechargement. `AStulWeapon` écoute directement son ASC et traduit les Gameplay Events `Reload.Commit`, `Reload.Completed` et `Reload.Cancelled` vers ses delegates Blueprint ; l'ability n'appelle plus de fonctions `NotifyReload...` redondantes ;
- après un délai prédit, seul le serveur applique `UStulWeaponAmmoRestoreEffect` à `CurrentAmmo`. Le client propriétaire suit localement la quantité attendue afin que les reloads incrémentaux progressent sans tenter d'appliquer un Gameplay Effect hors fenêtre de prédiction ; l'attribut autoritaire est ensuite répliqué au propriétaire ;
- les événements de reload sont remis directement au même ASC via `HandleGameplayEvent`, sans nouvelle recherche de composant depuis l'Actor. Après une restauration autoritaire réussie, l'ASC force une mise à jour réseau afin que l'effet instantané soit rapidement visible sur le client propriétaire ;
- la fin locale prédite d'un reload réussi ne réplique pas `EndAbility` vers le serveur : le minuteur client peut finir avant le minuteur autoritaire et ne doit pas interrompre la restauration serveur. Seule l'autorité réplique la terminaison réussie ;
- le mode `Full` attend une fois puis restaure toutes les munitions manquantes. Le mode `Custom` attend et restaure `BaseAmmoToReload` à chaque cycle jusqu'au plein ou jusqu'à une interruption ;
- démarrer un rechargement annule un tir actif et fait sortir de la visée, comme dans l'implémentation Boston de référence. Pendant un rechargement, une tentative de tir est bloquée sans annuler le reload, tandis qu'une nouvelle visée peut commencer sans l'interrompre. Le changement de mode de tir reste indépendant et n'annule aucune ability ;
- `UStulWeaponAmmoRestoreEffect` applique la magnitude positive `Stul.Weapon.SetByCaller.AmmoRestore` à `CurrentAmmo`. `OnReloadCommit` et `Stul.Weapon.Event.Reload.Commit` sont émis à chaque restauration atomique, ce qui permet de synchroniser une animation ou un son par cartouche ;
- les événements de fin distinguent `Reload.Completed` de `Reload.Cancelled` à partir de `bWasCancelled`, qui reste fiable lorsqu'une fin autoritaire arrive avant la fin locale prédite. Un rechargement incrémental interrompu conserve les cartouches déjà insérées ;
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
4. vérifier que l'input du projet passe uniquement par le Manager et n'appelle plus directement `CycleFireMode`.

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

## Vérifications en attente

- La compilation manuelle et le premier test PIE multijoueur ont validé les chemins exercés du Manager, de la Fast Array et de l'ASC. La matrice réseau complète décrite ci-dessus reste à exécuter.
- Le type `StulWeaponDefinition` doit être déclaré dans les `PrimaryAssetTypesToScan` du projet consommateur. Il a été ajouté au `Config/DefaultGame.ini` de Boston pour le banc de test ; cette exigence devra être documentée dans le README distribuable du plugin.
- `AStulWeaponTestController` traite les inputs dans `PostProcessInput` et récupère désormais le Manager aussi bien dans `OnPossess` côté serveur que dans `AcknowledgePossession` côté client. Il retente également la résolution si le composant apparaît tardivement.
- Les armes appartenant à un joueur distant sont marquées `AutonomousProxy` par le Manager après leur spawn. Sans cela, leur Avatar GAS reste `SimulatedProxy` sur le client propriétaire et `UGameplayAbility::CanActivateAbility` refuse toutes les activations locales, même avec un ActorInfo correctement local.
- Après le changement de type de `AvailableFireModes` vers `FGameplayTagContainer`, vérifier leur contenu et réenregistrer les Data Assets existants.
- Vérifier la référence `RecoilCurve` après son passage en soft reference.
- Rafraîchir les nœuds Blueprint utilisant `ProcessAbilityInput`, l’ancien `MakeInstanceData`, `Level` ou `Seed`.

## Convention de journalisation

- Tous les diagnostics runtime du plugin utilisent la catégorie globale `LogStulWeaponSystem`.
- `Error` signale un invariant cassé ou une opération essentielle qui a échoué.
- `Warning` signale une configuration invalide ou un appel incorrect qui demande une correction.
- `Verbose` et `VeryVerbose` servent aux états attendus et au diagnostic détaillé, notamment aux refus d’activation GAS dus aux conditions, coûts ou cooldowns.
- Aucun log répétitif ne doit être ajouté dans une boucle par frame pour un état attendu.
- Toute nouvelle fonctionnalité doit journaliser ses chemins d’erreur importants au moment de son implémentation afin d’éviter une passe de logs ultérieure.
- Les conditions `if` et les appels `UE_LOG` restent sur une seule ligne afin de respecter la convention de lisibilité du projet.
- La présentation des `.h` et `.cpp` suit les bannières de catégories définies dans `AGENTS.md`, qui constitue la référence principale des conventions de code. Ce fichier conserve seulement ce rappel, tandis que `AGENTS.md` porte la règle obligatoire.
