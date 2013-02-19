
backupdir=~gnats/backup

categories=$(awk -F: '/^[^#]/ { print $1 }' ~gnats/gnats-adm/categories)
classes=$(awk -F: '/^[^#]/ { print $1 }' ~gnats/gnats-adm/classes)
states=$(awk -F: '/^[^#]/ { print $1 }' ~gnats/gnats-adm/states)

