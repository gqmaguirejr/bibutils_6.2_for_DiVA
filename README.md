# bibutils_6.2_for_DiVA
A set of bibutils_6.2 for working with DiVA (specifically the KTH Publication
Database instance of DiVA).

Once you have the MODS file from DiVA you can say:
```
xml2bib file_name.mods > file_name.bib
```
     and you get the version of the abstract and keywords in English (can can also explicitly say --english)

or
```
xml2bib --swedish file_name.xml > file_name.bib
```
      and you get the Swedish version.

The titles are handled in a rather complex fashion, but it is fully explained
in the bibtexo.c file. I hope that I have the logic correct for this, as I
have only tested on a small set of test cases.

For example, in Swedish the title will be:
```
title="Virtuell verklighet f{\"o}r f{\"o}rmedling av ett f{\"o}retags produkter: En unders{\"o}kning av kunders beslutsprocess i valet av k{\"o}ksmoduler gjord ur ett l{\"a}randeperspektiv [Virtual Reality for communicating a company's products: A survey of customers' decision-making in the process of choosing kitchen modules made from a learning perspective] (med engelska sammanfattning)",
```
while in English the title will be:
```
title="Virtuell verklighet f{\"o}r f{\"o}rmedling av ett f{\"o}retags produkter: En unders{\"o}kning av kunders beslutsprocess i valet av k{\"o}ksmoduler gjord ur ett l{\"a}randeperspektiv [Virtual Reality for communicating a company's products: A survey of customers' decision-making in the process of choosing kitchen modules made from a learning perspective] (with English summary)",
```

With the first part of the title being in the language of the thesis, the
second the title in the other language, and the parenthetical saying that
there is an English summary of the thesis.
