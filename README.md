# 2pc
- offline/online 2PC

## Alex's notes
- `__m128i`
    - used for inputLabels, outputLabels, and wires
    - keys for DKC
- to look at json files
    - http://www.jsoneditoronline.org/

### TODO:
- saving garbled circuit to disk
- saving instructions to disk (err sending over network in char pointer bufffer)
    - http://stackoverflow.com/questions/19311568/msgpack-packing-char-array-in-c 

### The Gist
1. Init Phase
    1. Garbler generates a bunch of GCs
    2. labels them
    3. saves them to disk
    4. sends them to evaluator
2. Online Phase - Garbler Side
    1. garbler chooses fn from json files
    2. creates instructions from json function spec
    3. sends instructions to evaluator
3. Online Phase - Evaluator Side
    1. Receive instructions from garbler.
    2. Follow instructions. This step involves OT.
    3. Send output of function back to garbler.

