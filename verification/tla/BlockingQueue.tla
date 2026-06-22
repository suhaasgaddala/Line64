-------------------------- MODULE BlockingQueue --------------------------
EXTENDS FiniteSets, Naturals, Sequences

CONSTANTS Capacity, Values

VARIABLES queue, accepted, popped, closed

vars == <<queue, accepted, popped, closed>>

NoDuplicates(sequence) ==
    Cardinality({sequence[index] : index \in 1..Len(sequence)}) = Len(sequence)

Init ==
    /\ queue = <<>>
    /\ accepted = <<>>
    /\ popped = <<>>
    /\ closed = FALSE

Push(value) ==
    /\ ~closed
    /\ Len(queue) < Capacity
    /\ value \in Values
    /\ value \notin {accepted[index] : index \in 1..Len(accepted)}
    /\ queue' = Append(queue, value)
    /\ accepted' = Append(accepted, value)
    /\ UNCHANGED <<popped, closed>>

Pop ==
    /\ Len(queue) > 0
    /\ popped' = Append(popped, Head(queue))
    /\ queue' = Tail(queue)
    /\ UNCHANGED <<accepted, closed>>

Close ==
    /\ ~closed
    /\ closed' = TRUE
    /\ UNCHANGED <<queue, accepted, popped>>

Next ==
    \/ \E value \in Values : Push(value)
    \/ Pop
    \/ Close

Spec == Init /\ [][Next]_vars

TypeOK ==
    /\ Capacity \in Nat \ {0}
    /\ queue \in Seq(Values)
    /\ accepted \in Seq(Values)
    /\ popped \in Seq(Values)
    /\ closed \in BOOLEAN

Bounded == Len(queue) <= Capacity

Conservation == accepted = popped \o queue

UniqueAcceptance == NoDuplicates(accepted)

UniqueConsumption == NoDuplicates(popped)

=============================================================================
