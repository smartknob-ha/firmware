
files=$(git diff-tree -r --no-commit-id --summary a7530fb79e716a559d331de10eb86b2692b7b906 | \
awk -F' ' '{
  # ensure we are not trying to process deleted files
  # only process puml files
  # do not try to process our theme or custom config
  if ( $1 !~ /^delete$/ && $4 ~ /\.puml$/ && $4 !~ /(theme|config)\.puml$/ )
  {
    # only print the file name and strip newlines for spaces
    printf "%s ", $4
  }
}
END { print "" } # ensure we do print a newline at the end
')
echo $files
echo "files=${files}"