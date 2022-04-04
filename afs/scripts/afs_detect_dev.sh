echo "Find out to which device your cf card is connected."
echo "Unmount and disconnect your cf card ... <press return>"
read key
lsblk
lsblk > s1.txt
echo
echo "Now reinsert / reconnect your cf card and wait a few seconds ... <press return>"
read key
lsblk
lsblk > s2.txt
dev=$(diff s1.txt s2.txt | grep "sd[abcd][^1234]" | sed 's/^.*\(sd[a-f]\).*$/\1/')
if [ -z "$dev" ]; then 
    echo
    echo "That didn't work ..."
    echo "Enter device manually: "
    read dest
else
    echo
    echo "Your cf card is connected to:"
    dest="/dev/$dev"
    echo "$dest"
fi
