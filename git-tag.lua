-- pandoc filter to inject git tag into meta-data block
--
-- pandoc --lua-filter=git-tag.lua ...

function Meta(elem)
    elem.date = pandoc.pipe("git", {"describe", "--tags", "--always"}, "")
    return elem
end
