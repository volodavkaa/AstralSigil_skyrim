Scriptname AS_LegacyDiscovery extends ObjectReference

Quest Property LegacyQuest Auto
Book Property StormPlans Auto
Book Property CinderPlans Auto
MiscObject Property UnfinishedStaff Auto
Weapon Property Stormheart Auto
Weapon Property Cinderfang Auto
Int Property DiscoveryStage Auto
Bool Property RequireRead Auto

Event OnRead()
    RefreshDiscovery()
EndEvent

Event OnContainerChanged(ObjectReference akNewContainer, ObjectReference akOldContainer)
    If !RequireRead && akNewContainer == Game.GetPlayer()
        RefreshDiscovery()
    EndIf
EndEvent

Function RefreshDiscovery()
    If LegacyQuest.IsCompleted()
        Return
    EndIf
    If !LegacyQuest.IsRunning()
        If !LegacyQuest.Start()
            Return
        EndIf
    EndIf

    If LegacyQuest.GetStage() < 10
        LegacyQuest.SetStage(10)
        LegacyQuest.SetObjectiveDisplayed(10, True)
    EndIf
    If DiscoveryStage >= 20 && LegacyQuest.GetStage() < 20
        LegacyQuest.SetStage(20)
        LegacyQuest.SetObjectiveCompleted(10, True)
        LegacyQuest.SetObjectiveDisplayed(20, True)
    EndIf

    Actor player = Game.GetPlayer()
    If player.GetItemCount(StormPlans) > 0 && player.GetItemCount(CinderPlans) > 0
        If player.GetItemCount(UnfinishedStaff) > 0 || player.GetItemCount(Stormheart) > 0 || player.GetItemCount(Cinderfang) > 0
            LegacyQuest.SetObjectiveCompleted(10, True)
            LegacyQuest.SetObjectiveCompleted(20, True)
            LegacyQuest.SetStage(100)
            LegacyQuest.CompleteQuest()
            LegacyQuest.Stop()
        EndIf
    EndIf
EndFunction
