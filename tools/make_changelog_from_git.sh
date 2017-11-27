#!/bin/bash

function traverse() {
	for file in "$1"/*
	do
		if [[ -f ${file} ]] && [[ ${file} == *khash.h ]] ; then
			# Create history when needed
			git log --follow --pretty=format:"%B" ${file} > "ChangeLog"
		fi

		if [ -d "${file}" ] ; then
			ls | grep "_version.h"
	        traverse "${file}"
	    fi
	done
}


git log --follow --pretty=format:"%B" ${file} > "ChangeLog"

function main() {
	echo "Updating Changelog file..."
    traverse .
	echo "Changelog updated"
}

main "$1"
