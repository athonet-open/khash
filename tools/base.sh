#!/usr/bin/env bash

set -e

# Usage
usage(){
    PADDING=$(echo $(basename $0) | sed 's/./ /g' )
    echo "$(basename $0)"
    echo
    echo "download deps"
    echo
    echo "options:"
    echo "$PADDING -z          no checkout of repos"
    echo "$PADDING -p          pack debs"
    echo "$PADDING -i          install repos"
    echo "$PADDING -d outdir   output directory"
    echo "$PADDING -f          force removal of deps.d"
    exit
}

# Parse Optional Arguments
FORCE=false
HELP=false
ROOTPATH="$(git rev-parse --show-toplevel)"
ROOTNAME=$(basename $ROOTPATH)
INFILE="$ROOTPATH/deps"
OUTDIR="$ROOTPATH/deps.d"
INSTALL=false
CHECKOUT=true
DEBS=false

while getopts "zpifd:h" OPTION
do
	case $OPTION in
        z)
            CHECKOUT=false
            ;;
        p)
            DEBS=true
            ;;
        i)
            INSTALL=true
            ;;
        f)
            FORCE=true
            ;;
        d)
            OUTDIR=$(realpath $OPTARG)
            ;;
        h)
            HELP=true        # get help usage
            ;;
	esac
done

if [[ $HELP == true ]];then
    usage
fi

if [[ -e "$INFILE" && $CHECKOUT == true ]];then

    if [[ ! -d "$OUTDIR" ]];then
        mkdir -p $OUTDIR
    fi

    if [[ $FORCE == true ]];then
        for repo in $(ls $OUTDIR);do
            git clean -xdff $OUTDIR
        done
    fi


    while read name version giturl; do
        repo="$OUTDIR/$name"

        created=false

        if [[ ! -d "$repo" ]];then
            echo "[$ROOTNAME] Cloning Repository $name $giturl vs $version"
            git clone --branch "$version" --single-branch "$giturl" "$repo"   # clone+checkout
            created=true
        else
            # Threat as special the tags since they produce some kind of references
            REMOTE=$(git ls-remote $giturl | grep -F "tags/$version^{}" | cut -f1)
            if [[ -z "$REMOTE" ]];then
                REMOTE=$(git ls-remote $giturl | grep -F "heads/$version" | cut -f1)
            fi

            LOCAL=$(git ls-remote $repo | grep -P '[ \t]HEAD' | cut -f1)

            if [[ "$REMOTE" != "$LOCAL" ]];then
                echo "[!] local branch $name different from requested $version"
                echo "[!] REMOTE: ${REMOTE:-EMPTY}"
                echo "[!] LOCAL : ${LOCAL:-EMPTY}"
                exit 127
            else
                echo "[$ROOTNAME: $name] ok"
            fi
        fi

        if [ -f $repo/deps ];then
            echo "[$ROOTNAME: $name] Found deps: recurring..."
            cd $repo
            $ROOTPATH/tools/base.sh -d $OUTDIR
            cd $before_jump
        fi

        if [[ $created == true ]];then
            echo $name >> $OUTDIR/deps_order
        fi


    done < $INFILE
fi


# since checkout is optional fail if structure not consistent
if [[ ! -d "$OUTDIR" || ! -e "$INFILE" || ! -e "$OUTDIR/deps_order" ]];then
    echo "[!] deps.d structure not consistent"
    exit 127
fi
# must be root for next steps
if [[ $EUID -ne 0 ]]; then
    echo "[!] you must be root to install the deps"
    exit 127
fi


# install
if [[ $INSTALL == true ]];then
    while read repo; do
        cd $OUTDIR/$repo

        echo "[$ROOTNAME: $repo] mk install"
        bash tools/mk install || err=1

        cd -
    done < $OUTDIR/deps_order
fi

# debs
if [[ $DEBS == true ]];then
    while read repo; do
        cd $OUTDIR/$repo

        echo "[$ROOTNAME: $repo] mk mkdeb"
        bash tools/mk mkdeb || echo "[!] $repo/tool/mk not present"

        cd -
    done < $OUTDIR/deps_order

    debs=$(find $OUTDIR -name '*.deb')

    for deb in $debs;do
        echo "[$ROOTNAME self] moving $deb in $OUTDIR"
        mv $deb $OUTDIR
    done
fi

echo "[$ROOTNAME: self] ok"
