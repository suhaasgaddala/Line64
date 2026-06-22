---------------------------- MODULE SPSCQueue ----------------------------
EXTENDS FiniteSets, Integers, Naturals, Sequences

CONSTANTS Capacity, TotalOps, NoPos

CellIndices == 0..(Capacity - 1)

Positions(low, high) == IF low >= high THEN {} ELSE low..(high - 1)

ExpectedPrefix(length) ==
    IF length = 0 THEN <<>> ELSE [index \in 1..length |-> index]

VARIABLES slots,
          head,
          tail,
          producerPhase,
          producerPosition,
          consumerPhase,
          consumerPosition,
          consumerValue,
          consumed

vars == <<slots, head, tail, producerPhase, producerPosition,
          consumerPhase, consumerPosition, consumerValue, consumed>>

Init ==
    /\ slots = [index \in CellIndices |-> 0]
    /\ head = 0
    /\ tail = 0
    /\ producerPhase = "idle"
    /\ producerPosition = NoPos
    /\ consumerPhase = "idle"
    /\ consumerPosition = NoPos
    /\ consumerValue = 0
    /\ consumed = <<>>

ProducerWrite ==
    /\ producerPhase = "idle"
    /\ head < TotalOps
    /\ head - tail < Capacity
    /\ slots' = [slots EXCEPT ![head % Capacity] = head + 1]
    /\ producerPhase' = "written"
    /\ producerPosition' = head
    /\ UNCHANGED <<head, tail, consumerPhase, consumerPosition,
                    consumerValue, consumed>>

ProducerPublish ==
    /\ producerPhase = "written"
    /\ producerPosition = head
    /\ head' = head + 1
    /\ producerPhase' = "idle"
    /\ producerPosition' = NoPos
    /\ UNCHANGED <<slots, tail, consumerPhase, consumerPosition,
                    consumerValue, consumed>>

ConsumerCopy ==
    /\ consumerPhase = "idle"
    /\ tail < head
    /\ consumerPhase' = "copied"
    /\ consumerPosition' = tail
    /\ consumerValue' = slots[tail % Capacity]
    /\ UNCHANGED <<slots, head, tail, producerPhase,
                    producerPosition, consumed>>

ConsumerRelease ==
    /\ consumerPhase = "copied"
    /\ consumerPosition = tail
    /\ consumed' = Append(consumed, consumerValue)
    /\ tail' = tail + 1
    /\ consumerPhase' = "idle"
    /\ consumerPosition' = NoPos
    /\ consumerValue' = 0
    /\ UNCHANGED <<slots, head, producerPhase, producerPosition>>

Next == ProducerWrite \/ ProducerPublish \/ ConsumerCopy \/ ConsumerRelease

Spec == Init /\ [][Next]_vars

TypeOK ==
    /\ Capacity \in Nat \ {0}
    /\ TotalOps \in Nat
    /\ slots \in [CellIndices -> 0..TotalOps]
    /\ head \in 0..TotalOps
    /\ tail \in 0..TotalOps
    /\ producerPhase \in {"idle", "written"}
    /\ producerPosition \in 0..TotalOps \cup {NoPos}
    /\ consumerPhase \in {"idle", "copied"}
    /\ consumerPosition \in 0..TotalOps \cup {NoPos}
    /\ consumerValue \in 0..TotalOps
    /\ consumed \in Seq(1..TotalOps)

CapacityInvariant == 0 <= tail /\ tail <= head /\ head - tail <= Capacity

ConsumedInOrder == consumed = ExpectedPrefix(tail)

CopyOwnsTail ==
    consumerPhase = "copied" =>
        /\ consumerPosition = tail
        /\ consumerValue = tail + 1

PublishedSlotsMatch ==
    \A position \in Positions(tail, head) :
        slots[position % Capacity] = position + 1

=============================================================================
