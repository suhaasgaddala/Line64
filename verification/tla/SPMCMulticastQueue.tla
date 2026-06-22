----------------------- MODULE SPMCMulticastQueue -----------------------
EXTENDS FiniteSets, Integers, Naturals, Sequences

CONSTANTS Capacity, Consumers, TotalPublications

CellIndices == 0..(Capacity - 1)

Oldest(sequence) == IF sequence >= Capacity THEN sequence - Capacity + 1 ELSE 1

Retained(sequence) ==
    IF sequence = 0 THEN {} ELSE Oldest(sequence)..sequence

StrictlyIncreasing(sequence) ==
    \A left, right \in 1..Len(sequence) :
        left < right => sequence[left] < sequence[right]

VARIABLES active,
          published,
          slots,
          nextSequence,
          registeredAt,
          reads,
          lagEvents

vars == <<active, published, slots, nextSequence, registeredAt, reads, lagEvents>>

Init ==
    /\ active = {}
    /\ published = 0
    /\ slots = [index \in CellIndices |-> 0]
    /\ nextSequence = [consumer \in Consumers |-> 0]
    /\ registeredAt = [consumer \in Consumers |-> 0]
    /\ reads = [consumer \in Consumers |-> <<>>]
    /\ lagEvents = [consumer \in Consumers |-> 0]

Register(consumer) ==
    /\ consumer \notin active
    /\ active' = active \cup {consumer}
    /\ nextSequence' = [nextSequence EXCEPT ![consumer] = published + 1]
    /\ registeredAt' = [registeredAt EXCEPT ![consumer] = published + 1]
    /\ UNCHANGED <<published, slots, reads, lagEvents>>

Publish ==
    /\ published < TotalPublications
    /\ LET sequence == published + 1
           index == (sequence - 1) % Capacity
       IN /\ slots' = [slots EXCEPT ![index] = sequence]
          /\ published' = sequence
    /\ UNCHANGED <<active, nextSequence, registeredAt, reads, lagEvents>>

ReportLag(consumer) ==
    /\ consumer \in active
    /\ nextSequence[consumer] <= published
    /\ nextSequence[consumer] < Oldest(published)
    /\ nextSequence' = [nextSequence EXCEPT ![consumer] = Oldest(published)]
    /\ lagEvents' = [lagEvents EXCEPT ![consumer] = @ + 1]
    /\ UNCHANGED <<active, published, slots, registeredAt, reads>>

Read(consumer) ==
    /\ consumer \in active
    /\ nextSequence[consumer] <= published
    /\ nextSequence[consumer] >= Oldest(published)
    /\ slots[(nextSequence[consumer] - 1) % Capacity] = nextSequence[consumer]
    /\ reads' = [reads EXCEPT
                    ![consumer] = Append(@, nextSequence[consumer])]
    /\ nextSequence' = [nextSequence EXCEPT ![consumer] = @ + 1]
    /\ UNCHANGED <<active, published, slots, registeredAt, lagEvents>>

Next ==
    \/ \E consumer \in Consumers : Register(consumer)
    \/ Publish
    \/ \E consumer \in Consumers : ReportLag(consumer)
    \/ \E consumer \in Consumers : Read(consumer)

Spec == Init /\ [][Next]_vars

TypeOK ==
    /\ Capacity \in Nat \ {0}
    /\ published \in 0..TotalPublications
    /\ active \subseteq Consumers
    /\ slots \in [CellIndices -> 0..TotalPublications]
    /\ nextSequence \in [Consumers -> 0..(TotalPublications + 1)]
    /\ registeredAt \in [Consumers -> 0..(TotalPublications + 1)]
    /\ reads \in [Consumers -> Seq(1..TotalPublications)]
    /\ lagEvents \in [Consumers -> Nat]

RegistrationInvariant ==
    \A consumer \in Consumers :
        IF consumer \in active
        THEN /\ registeredAt[consumer] >= 1
             /\ nextSequence[consumer] >= registeredAt[consumer]
             /\ nextSequence[consumer] <= published + 1
        ELSE /\ registeredAt[consumer] = 0
             /\ nextSequence[consumer] = 0
             /\ reads[consumer] = <<>>

ReadsAreOrdered ==
    \A consumer \in Consumers : StrictlyIncreasing(reads[consumer])

ReadsRespectRegistration ==
    \A consumer \in active :
        \A index \in 1..Len(reads[consumer]) :
            /\ reads[consumer][index] >= registeredAt[consumer]
            /\ reads[consumer][index] <= published

RetainedSlotsMatch ==
    \A sequence \in Retained(published) :
        slots[(sequence - 1) % Capacity] = sequence

=============================================================================
