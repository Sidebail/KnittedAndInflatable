#pragma once

#include <CoreMinimal.h>
#include <ISequencer.h>
#include <Launch/Resources/Version.h>

/**
 * When a level sequence is closed or an actor is removed from a level sequence, the actor's state is restored to the
 * state from before the level sequence was opened. When this happens, the level sequence manager reapplies the server
 * state so changes made by other users while the level sequencer was open are not lost.
 */
class sfLevelSequenceManager
{
public:
    /**
     * @return  sfLevelSequenceManager& static singleton instance
     */
    static sfLevelSequenceManager& Get();

    /**
     * Constructor
     */
    sfLevelSequenceManager();

    /**
     * Initialization
     */
    void Initialize();

    /**
     * Called every frame.
     */
    void Update();

private:

    /**
     * Delete event listener called when uobjects are garbage collected.
     */
    class DeleteEventListener : public FUObjectArray::FUObjectDeleteListener
    {
    public:
        /**
         * Called when a uobject is garbage collected. If it is an actor, removes it from the sequenced actors map.
         *
         * @param   const class UOjectBase* uobjPtr that was garbage collected.
         * @param   int index of uobject in global uobject array.
         */
        virtual void NotifyUObjectDeleted(const class UObjectBase *uobjPtr, int index) override;

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 23
        /**
         * Called when UObject Array is being shut down, this is where all listeners should be removed from it.
         */
        virtual void OnUObjectArrayShutdown() override;
#endif
    };

    TArray<ISequencer*> m_newSequencers;
    TSet<ISequencer*> m_toRestore;
    // maps sequencers to actors that are controlled by those sequencers
    TMap<ISequencer*, TArray<AActor*>> m_sequencedActors;
    DeleteEventListener m_deleteListener;

    /**
     * Called when a sequencer is opened. Registers event listeners on the sequencer and adds it to the array of new
     * sequencers to have its actors added to the sequenced actors map next update.
     *
     * @param   ISequencer* sequencerPtr that was opened.
     */
    void OnCreateSequencer(ISequencer* sequencerPtr);

    /**
     * Restores actors controlled by a sequencer to the sever state.
     *
     * @param   ISequencer* sequencerPtr to restore controlled actors for.
     */
    void RestoreActors(ISequencer* sequencerPtr);

    /**
     * Restores a uobject and its component children to the server state recursively.
     *
     * @param   sfObject::SPtr objPtr with the sever state.
     * @param   UObject* uobjPtr to restore to the server state.
     */
    void Restore(sfObject::SPtr objPtr, UObject* uobjPtr);
};
