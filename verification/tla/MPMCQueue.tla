---------------------------- MODULE MPMCQueue ----------------------------
EXTENDS FiniteSets, Integers, Naturals, Sequences

CONSTANTS Capacity, PushOps, PopOps, NoPosition, NoValue

CellIndices == 0..(Capacity - 1)

VARIABLES cells,
          enqueuePosition,
          dequeuePosition,
          pushPhase,
          pushPosition,
          popPhase,
          popPosition,
          popValue

PushStarted == {operation \in PushOps : pushPhase[operation] # "idle"}
PopStarted == {operation \in PopOps : popPhase[operation] # "idle"}

PushPositions == {pushPosition[operation] : operation \in PushStarted}
PopPositions == {popPosition[operation] : operation \in PopStarted}

PrefixPositions(count) == IF count = 0 THEN {} ELSE 0..(count - 1)

vars == <<cells, enqueuePosition, dequeuePosition,
          pushPhase, pushPosition, popPhase, popPosition, popValue>>

Init ==
    /\ cells = [index \in CellIndices |-> [sequence |-> index, value |-> NoValue]]
    /\ enqueuePosition = 0
    /\ dequeuePosition = 0
    /\ pushPhase = [operation \in PushOps |-> "idle"]
    /\ pushPosition = [operation \in PushOps |-> NoPosition]
    /\ popPhase = [operation \in PopOps |-> "idle"]
    /\ popPosition = [operation \in PopOps |-> NoPosition]
    /\ popValue = [operation \in PopOps |-> NoValue]

PushClaim(operation) ==
    /\ pushPhase[operation] = "idle"
    /\ LET position == enqueuePosition
           index == position % Capacity
       IN /\ cells[index].sequence = position
          /\ enqueuePosition' = position + 1
          /\ pushPhase' = [pushPhase EXCEPT ![operation] = "claimed"]
          /\ pushPosition' = [pushPosition EXCEPT ![operation] = position]
    /\ UNCHANGED <<cells, dequeuePosition, popPhase, popPosition, popValue>>

PushWrite(operation) ==
    /\ pushPhase[operation] = "claimed"
    /\ LET index == pushPosition[operation] % Capacity
       IN cells' = [cells EXCEPT ![index].value = operation]
    /\ pushPhase' = [pushPhase EXCEPT ![operation] = "written"]
    /\ UNCHANGED <<enqueuePosition, dequeuePosition, pushPosition,
                    popPhase, popPosition, popValue>>

PushPublish(operation) ==
    /\ pushPhase[operation] = "written"
    /\ LET position == pushPosition[operation]
           index == position % Capacity
       IN cells' = [cells EXCEPT ![index].sequence = position + 1]
    /\ pushPhase' = [pushPhase EXCEPT ![operation] = "published"]
    /\ UNCHANGED <<enqueuePosition, dequeuePosition, pushPosition,
                    popPhase, popPosition, popValue>>

PopClaim(operation) ==
    /\ popPhase[operation] = "idle"
    /\ LET position == dequeuePosition
           index == position % Capacity
       IN /\ cells[index].sequence = position + 1
          /\ dequeuePosition' = position + 1
          /\ popPhase' = [popPhase EXCEPT ![operation] = "claimed"]
          /\ popPosition' = [popPosition EXCEPT ![operation] = position]
          /\ popValue' = [popValue EXCEPT ![operation] = cells[index].value]
    /\ UNCHANGED <<cells, enqueuePosition, pushPhase, pushPosition>>

PopRelease(operation) ==
    /\ popPhase[operation] = "claimed"
    /\ LET position == popPosition[operation]
           index == position % Capacity
       IN cells' = [cells EXCEPT ![index].sequence = position + Capacity]
    /\ popPhase' = [popPhase EXCEPT ![operation] = "released"]
    /\ UNCHANGED <<enqueuePosition, dequeuePosition, pushPhase,
                    pushPosition, popPosition, popValue>>

Next ==
    \/ \E operation \in PushOps : PushClaim(operation)
    \/ \E operation \in PushOps : PushWrite(operation)
    \/ \E operation \in PushOps : PushPublish(operation)
    \/ \E operation \in PopOps : PopClaim(operation)
    \/ \E operation \in PopOps : PopRelease(operation)

Spec == Init /\ [][Next]_vars

TypeOK ==
    /\ Capacity \in Nat \ {0, 1}
    /\ cells \in [CellIndices ->
                    [sequence : 0..(Cardinality(PushOps) + Capacity),
                     value : PushOps \cup {NoValue}]]
    /\ enqueuePosition \in 0..Cardinality(PushOps)
    /\ dequeuePosition \in 0..Cardinality(PopOps)
    /\ pushPhase \in [PushOps -> {"idle", "claimed", "written", "published"}]
    /\ pushPosition \in [PushOps ->
                           0..Cardinality(PushOps) \cup {NoPosition}]
    /\ popPhase \in [PopOps -> {"idle", "claimed", "released"}]
    /\ popPosition \in [PopOps ->
                          0..Cardinality(PopOps) \cup {NoPosition}]
    /\ popValue \in [PopOps -> PushOps \cup {NoValue}]

PositionBounds ==
    /\ dequeuePosition <= enqueuePosition
    /\ enqueuePosition - Cardinality({operation \in PopOps :
                                        popPhase[operation] = "released"})
       <= Capacity

UniquePushClaims ==
    /\ Cardinality(PushPositions) = Cardinality(PushStarted)
    /\ PushPositions = PrefixPositions(enqueuePosition)

UniquePopClaims ==
    /\ Cardinality(PopPositions) = Cardinality(PopStarted)
    /\ PopPositions = PrefixPositions(dequeuePosition)

PopMatchesPush ==
    \A consumerOperation \in PopStarted :
        \E producerOperation \in PushStarted :
            /\ pushPosition[producerOperation] = popPosition[consumerOperation]
            /\ popValue[consumerOperation] = producerOperation

NoDuplicatePops ==
    Cardinality({popValue[operation] : operation \in PopStarted}) =
        Cardinality(PopStarted)

PublishedBeforePop ==
    \A consumerOperation \in PopStarted :
        \E producerOperation \in PushOps :
            /\ pushPosition[producerOperation] = popPosition[consumerOperation]
            /\ pushPhase[producerOperation] = "published"

=============================================================================
