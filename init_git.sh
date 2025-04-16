echo "# 5600 project/n collaborated by Mu Xihe, Liu Yangyi and Wang Yange" >> README.md
git init
git remote add origin https://github.com/moksaiho/5600Project.git
git pull origin main
git add .
git commit -m "first commit"
git branch -M main
git push -u origin main