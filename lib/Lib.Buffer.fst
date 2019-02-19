module Lib.Buffer

open FStar.Mul

open Lib.IntTypes
open Lib.RawIntTypes

friend Lib.Sequence
friend Lib.LoopCombinators

module B = LowStar.Buffer
module IB = LowStar.ImmutableBuffer
module U32 = FStar.UInt32
module ST = FStar.HyperStack.ST
module HS = FStar.HyperStack
module Seq = Lib.Sequence
module ByteSeq = Lib.ByteSequence

#reset-options "--z3rlimit 100 --max_fuel 0 --max_ifuel 0"

let modifies_preserves_live #t #a b l h0 h1 = ()
let modifies_includes l1 l2 h0 h1 = ()
let modifies_trans l1 l2 h0 h1 h2 = ()
let live_sub #t #a #len b start n h = ()
let modifies_sub #t #a #len b start n h0 h1 = ()

let as_seq_gsub #t #a #len h b start n = ()

let sub #t #a #len b start n =
  match t with
  | MUT -> B.sub (b <: buffer a) start n
  | IMMUT -> IB.isub (b <: ibuffer a) start n

let index #t #a #len b i =
  match t with
  | MUT -> B.index (b <: buffer a) i
  | IMMUT -> IB.index (b <: ibuffer a) i

let upd #a #len b i v =
  let h0 = ST.get() in
  B.upd (b <: buffer a) i v;
  let h1 = ST.get() in
  assert (B.modifies (loc b) h0 h1);
  assert (modifies (loc b) h0 h1)

let bget_as_seq #t #a #len h b i = ()

let create #a clen init =
  B.alloca init (normalize_term clen)

let createL #a init =
  B.alloca_of_list init

#set-options "--max_fuel 1"

let createL_global #a init =
  IB.igcmalloc_of_list #a root init

let recall_contents #a #len b s =
  B.recall_p (b <: ibuffer a) (cpred s)

let copy #t #a #len o i =
  match t with
  | MUT ->
    let h0 = ST.get () in
    LowStar.BufferOps.blit (i <: buffer a) 0ul (o <: buffer a) 0ul len;
    let h1 = ST.get () in
    assert (Seq.slice (as_seq h1 o) 0 (v len) == Seq.slice (as_seq h0 i) 0 (v len))
  | IMMUT ->
    let h0 = ST.get () in
    LowStar.BufferOps.blit (i <: ibuffer a) 0ul (o <: buffer a) 0ul len;
    let h1 = ST.get () in
    assert (Seq.slice (as_seq h1 o) 0 (v len) == Seq.slice (as_seq h0 i) 0 (v len))

let memset #a #blen b init len =
  B.fill #a #(fun _ _ -> True) #(fun _ _ -> True) b init len

#set-options "--max_fuel 0"

let update_sub #t #a #len dst start n src =
  match t with
  | MUT ->
  let h0 = ST.get () in
  LowStar.BufferOps.blit (src <: buffer a) 0ul (dst <: buffer a) (size_to_UInt32 start) (size_to_UInt32 n);
  let h1 = ST.get () in
  assert (forall (k:nat{k < v n}). bget h1 dst (v start + k) == bget h0 src k);
  FStar.Seq.lemma_eq_intro
    (as_seq h1 dst)
    (Seq.update_sub #a #(v len) (as_seq h0 dst) (v start) (v n) (as_seq h0 src))
  | IMMUT ->
  let h0 = ST.get () in
  LowStar.BufferOps.blit (src <: ibuffer a) 0ul (dst <: buffer a) (size_to_UInt32 start) (size_to_UInt32 n);
  let h1 = ST.get () in
  assert (forall (k:nat{k < v n}). bget h1 dst (v start + k) == bget h0 src k);
  FStar.Seq.lemma_eq_intro
    (as_seq h1 dst)
    (Seq.update_sub #a #(v len) (as_seq h0 dst) (v start) (v n) (as_seq h0 src))

let update_sub_f #a #len h0 buf start n spec f =
  let tmp = sub buf start n in
  let h0 = ST.get () in
  f tmp;
  let h1 = ST.get () in
  assert (v (len -! (start +! n)) == v len - v (start +! n));
  B.modifies_buffer_elim (B.gsub #a buf 0ul start) (loc tmp) h0 h1;
  B.modifies_buffer_elim (B.gsub #a buf (start +! n) (len -! (start +! n))) (loc tmp) h0 h1;
  Sequence.lemma_update_sub (as_seq h0 buf) (v start) (v n) (spec h0) (as_seq h1 buf)

let concat2 #a #t0 #t1 len0 s0 len1 s1 s =
  let h0 = ST.get () in
  update_sub s (size 0) len0 s0;
  update_sub s len0 len1 s1;
  let h1 = ST.get () in
  Seq.eq_intro (Seq.sub (as_seq h1 s) 0 (v len0)) (as_seq h0 s0);
  Seq.lemma_concat2 (v len0) (as_seq h0 s0) (v len1) (as_seq h0 s1) (as_seq h1 s)

let concat3 #a #t0 #t1 #t2 len0 s0 len1 s1 len2 s2 s =
  let h0 = ST.get () in
  update_sub s (size 0) len0 s0;
  update_sub s len0 len1 s1;
  let h1 = ST.get () in
  Seq.eq_intro (Seq.sub (as_seq h1 s) 0 (v len0)) (as_seq h0 s0);
  update_sub s (len0 +! len1) len2 s2;
  let h2 = ST.get () in
  Seq.eq_intro (Seq.sub (as_seq h2 s) 0 (v len0)) (as_seq h0 s0);
  Seq.eq_intro (Seq.sub (as_seq h2 s) (v len0) (v len1)) (as_seq h0 s1);
  Seq.lemma_concat3 (v len0) (as_seq h0 s0) (v len1) (as_seq h0 s1) (v len2) (as_seq h0 s2) (as_seq h2 s)

let loop_nospec #h0 #a #len n buf impl =
  let inv h1 j = modifies (loc buf) h0 h1 in
  Lib.Loops.for (size 0) n inv impl

let loop_range_nospec #h0 #a #len start n buf impl =
  let inv h1 j = modifies (loc buf) h0 h1 in
  Lib.Loops.for start (start +. n) inv impl

#set-options "--max_fuel 1"

let loop h0 n a_spec refl footprint spec impl =
  let inv h i = loop_inv h0 n a_spec refl footprint spec i h in
  Lib.Loops.for (size 0) n inv impl

let loop1 #b #blen h0 n acc spec impl =
  let inv h i = loop1_inv h0 n b blen acc spec i h in
  Lib.Loops.for (size 0) n inv impl

let loop2 #b0 #blen0 #b1 #blen1 h0 n acc0 acc1 spec impl =
  let inv h i = loop2_inv #b0 #blen0 #b1 #blen1 h0 n acc0 acc1 spec i h in
  Lib.Loops.for (size 0) n inv impl

#set-options "--max_fuel 0"

let salloc1 #a #res h len x footprint spec spec_inv impl =
  let h0 = ST.get() in
  push_frame();
  let h1 = ST.get() in
  B.fresh_frame_modifies h0 h1;
  let b = B.alloca x len in
  let h2 = ST.get() in
  let r = impl b in
  let h3 = ST.get() in
  memset #a #len b x len;
  let h4 = ST.get() in
  pop_frame();
  let h5 = ST.get() in
  B.popped_modifies h4 h5;
  spec_inv h2 h3 h5 r;
  r

inline_for_extraction noextract
let salloc1_trivial #a #res h len x footprint spec impl =
  salloc1 #a #res h len x footprint spec
    (fun h1 h2 h3 (r:res) -> assert (spec r h2); assert (spec r h3))
    impl

inline_for_extraction noextract
let salloc_nospec #a #res h len x footprint impl =
  salloc1_trivial #a #res h len x footprint (fun _ _ -> True) impl

inline_for_extraction noextract
val loopi_blocks_f:
    #a:Type0
  -> #b:Type0
  -> #blen:size_t
  -> blocksize:size_t{v blocksize > 0}
  -> inpLen:size_t
  -> inp:lbuffer a inpLen
  -> spec_f:(i:nat{i < v inpLen / v blocksize}
              -> Seq.lseq a (v blocksize)
              -> Seq.lseq b (v blen)
              -> Seq.lseq b (v blen))
  -> f:(i:size_t{v i < v inpLen / v blocksize}
       -> inp:lbuffer a blocksize
       -> w:lbuffer b blen -> Stack unit
          (requires fun h ->
            live h inp /\ live h w /\ disjoint inp w)
          (ensures  fun h0 _ h1 ->
            modifies (loc w) h0 h1 /\
            as_seq h1 w == spec_f (v i) (as_seq h0 inp) (as_seq h0 w)))
  -> nb:size_t{v nb == v inpLen / v blocksize}
  -> i:size_t{v i < v nb}
  -> w:lbuffer b blen ->
  Stack unit
    (requires fun h -> live h inp /\ live h w /\ disjoint inp w)
    (ensures  fun h0 _ h1 ->
      modifies (loc w) h0 h1 /\
      as_seq h1 w ==
      Sequence.repeati_blocks_f (v blocksize) (as_seq h0 inp) spec_f (v nb) (v i) (as_seq h0 w))
let loopi_blocks_f #a #b #blen bs inpLen inp spec_f f nb i w =
  Math.Lemmas.lemma_mult_lt_right (v bs) (v i) (v nb);
  assert ((v i + 1) * v bs == v i * v bs + v bs);
  assert (v i * v bs + v bs <= v nb * v bs);
  let block = sub inp (i *. bs) bs in
  f i block w

inline_for_extraction noextract
val loopi_blocks_f_nospec:
    #a:Type0
  -> #b:Type0
  -> #blen:size_t
  -> blocksize:size_t{v blocksize > 0}
  -> inpLen:size_t
  -> inp:lbuffer a inpLen
  -> f:(i:size_t{v i < v inpLen / v blocksize}
       -> inp:lbuffer a blocksize
       -> w:lbuffer b blen -> Stack unit
          (requires fun h ->
            live h inp /\ live h w /\ disjoint inp w)
          (ensures  fun h0 _ h1 ->
            modifies (loc w) h0 h1))
  -> nb:size_t{v nb == v inpLen / v blocksize}
  -> i:size_t{v i < v nb}
  -> w:lbuffer b blen ->
  Stack unit
    (requires fun h -> live h inp /\ live h w /\ disjoint inp w)
    (ensures  fun h0 _ h1 -> modifies (loc w) h0 h1)

let loopi_blocks_f_nospec #a #b #blen bs inpLen inp f nb i w =
  assert ((v i + 1) * v bs <= v nb * v bs);
  let block = sub inp (i *. bs) bs in
  f i block w

let loopi_blocks #a #b #blen bs inpLen inp spec_f spec_l f l w =
  let nb = inpLen /. bs in
  let rem = inpLen %. bs in
  [@ inline_let]
  let spec_fh h0 = Seq.repeati_blocks_f (v bs) (as_seq h0 inp) spec_f (v nb) in
  let h0 = ST.get () in
  loop1 #b #blen h0 nb w spec_fh
  (fun i ->
    Loop.unfold_repeati (v nb) (spec_fh h0) (as_seq h0 w) (v i);
    loopi_blocks_f #a #b #blen bs inpLen inp spec_f f nb i w);
  let last = sub #_ #_ #inpLen  inp (nb *. bs) rem in
  l nb rem last w

let loopi_blocks_nospec #a #b #blen bs inpLen inp f l w =
  let nb = inpLen /. bs in
  let rem = inpLen %. bs in
  let h0 = ST.get () in
  loop_nospec #h0 #b #blen nb w
  (fun i -> loopi_blocks_f_nospec #a #b #blen bs inpLen inp f nb i w);
  let last = sub inp (nb *. bs) rem in
  l nb rem last w

inline_for_extraction noextract
val loop_blocks_f:
    #a:Type0
  -> #b:Type0
  -> #blen:size_t
  -> blocksize:size_t{v blocksize > 0}
  -> inpLen:size_t
  -> inp:lbuffer a inpLen
  -> spec_f:(Seq.lseq a (v blocksize)
              -> Seq.lseq b (v blen)
              -> Seq.lseq b (v blen))
  -> f:(inp:lbuffer a blocksize
       -> w:lbuffer b blen -> Stack unit
          (requires fun h ->
            live h inp /\ live h w /\ disjoint inp w)
          (ensures  fun h0 _ h1 ->
            modifies (loc w) h0 h1 /\
            as_seq h1 w == spec_f (as_seq h0 inp) (as_seq h0 w)))
  -> nb:size_t{v nb == v inpLen / v blocksize}
  -> i:size_t{v i < v nb}
  -> w:lbuffer b blen ->
  Stack unit
    (requires fun h -> live h inp /\ live h w /\ disjoint inp w)
    (ensures  fun h0 _ h1 ->
      modifies (loc w) h0 h1 /\
      as_seq h1 w ==
      Sequence.repeat_blocks_f (v blocksize) (as_seq h0 inp) spec_f (v nb) (v i) (as_seq h0 w))

let loop_blocks_f #a #b #blen bs inpLen inp spec_f f nb i w =
  assert ((v i + 1) * v bs <= v nb * v bs);
  let block = sub #_ #_ #inpLen inp (i *. bs) bs in
  f block w

let loop_blocks #a #b #blen bs inpLen inp spec_f spec_l f l w =
  let nb = inpLen /. bs in
  let rem = inpLen %. bs in
  [@ inline_let]
  let spec_fh h0 = Seq.repeat_blocks_f (v bs) (as_seq h0 inp) spec_f (v nb) in
  let h0 = ST.get () in
  loop1 #b #blen h0 nb w spec_fh
  (fun i ->
    Loop.unfold_repeati (v nb) (spec_fh h0) (as_seq h0 w) (v i);
    loop_blocks_f #a #b #blen bs inpLen inp spec_f f nb i w);
  let last = sub #_ #_ #inpLen inp (nb *. bs) rem in
  l rem last w

#set-options "--max_fuel 1"

let fill_blocks #t h0 len n output a_spec refl footprint spec impl =
  [@inline_let]
  let a_spec' (i:nat{i <= v n}) =
    Math.Lemmas.lemma_mult_le_right (v len) i (v n);
    a_spec i & Seq.lseq t (i * v len) in
  [@inline_let]
  let refl' h (i:nat{i <= v n}) : GTot (a_spec' i) =
    Math.Lemmas.lemma_mult_le_right (v len) i (v n);
    refl h i, as_seq h (gsub output (size 0) (size i *! len))
  in
  let footprint' i = B.loc_union (footprint i) (loc output) in
  [@inline_let]
  let spec' h0 : GTot (i:nat{i < v n} -> a_spec' i -> a_spec' (i + 1)) =
    let f = spec h0 in
    fun i so ->
      Math.Lemmas.lemma_mult_le_right (v len) i (v n);
      let s, o = so <: a_spec i & Seq.lseq t (i * v len) in
      let s', block = f i s in
      let o' : Seq.lseq t ((i + 1) * v len) = Seq.(o @| block) in
      s', o'
  in
  let h0 = ST.get () in
  loop h0 n a_spec' refl' footprint' spec'
  (fun i ->
    Loop.unfold_repeat_gen (v n) a_spec' (spec' h0) (refl' h0 0) (v i);
    let block = sub output (i *! len) len in
    impl i block;
    let h = ST.get() in
    B.loc_includes_union_l (footprint (v i + 1)) (loc output) (loc block);
    B.loc_includes_union_l (footprint (v i + 1)) (loc output) (footprint (v i + 1));
    assert ((v i + 1) * v len == v i * v len + v len);
    FStar.Seq.lemma_split
      (as_seq h (gsub output (size 0) (i *! len +! len)))
      (v i * v len)
  );
  assert (Seq.equal
    (as_seq h0 (gsub output (size 0) (size 0 *! len))) FStar.Seq.empty);
  assert_norm (
    Seq.generate_blocks (v len) (v n) a_spec (spec h0) (refl h0 0) ==
    norm [delta] Seq.generate_blocks (v len) (v n) a_spec (spec h0) (refl h0 0))

let fillT #a clen o spec_f f =
  let open Seq in
  let h0 = ST.get () in
  [@inline_let]
  let a_spec = createi_a a (v clen) spec_f  in
  [@inline_let]
  let refl h i = sub (as_seq h o) 0 i in
  [@inline_let]
  let footprint i = loc o in
  [@inline_let]
  let spec h = createi_step a (v clen) spec_f in
  eq_intro (of_list []) (refl h0 0);
  loop h0 clen a_spec refl footprint spec
    (fun i ->
      Loop.unfold_repeat_gen (v clen) a_spec (spec h0) (refl h0 0) (v i);
      o.(i) <- f i;
      let h' = ST.get () in
      FStar.Seq.lemma_split (as_seq h' o) (v i)
    )

let fill #a h0 clen o spec impl =
  let open Seq in
  let h0 = ST.get() in
  [@inline_let]
  let a_spec = createi_a a (v clen) (spec h0) in
  [@inline_let]
  let refl h i = sub (as_seq h o) 0 i in
  [@inline_let]
  let footprint i = loc o in
  let spec' = spec in
  [@inline_let]
  let spec h = createi_step a (v clen) (spec h0) in
  eq_intro (of_list []) (refl h0 0);
  loop h0 clen a_spec refl footprint spec
  (fun i ->
    Loop.unfold_repeat_gen (v clen) a_spec (spec h0) (refl h0 0) (v i);
    impl i;
    let h' = ST.get () in
    FStar.Seq.lemma_split (as_seq h' o) (v i)
  )

#set-options "--max_fuel 0"

let mapT #t #a #b clen out f inp =
  let h0 = ST.get () in
  fill h0 clen out
    (fun h -> let in_seq = as_seq h inp in Seq.map_inner f in_seq)
    (fun i -> out.(i) <- f inp.(i))

let mapiT #t #a #b clen out f inp =
  let h0 = ST.get () in
  fill h0 clen out
    (fun h -> let in_seq = as_seq h inp in Seq.mapi_inner (fun i -> f (size i)) in_seq)
    (fun i -> let xi = inp.(i) in out.(i) <- f i xi)

let mapi #a #b h0 clen out spec_f f inp = 
  let h0 = ST.get () in
  fill h0 clen out
    (fun h -> let in_seq = as_seq h inp in Seq.mapi_inner (spec_f h0) in_seq)
    (fun i -> let xi = inp.(i) in out.(i) <- f i xi)
